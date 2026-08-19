use serde::{Deserialize, Serialize};
use std::collections::{HashMap, HashSet};
use std::io::{self, Read};
use std::path::Path;
use wellen::{FileFormat, Hierarchy, ScopeRef, SignalRef, VarRef};

const REQUEST_SCHEMA: &str = "wave-wellen-reader.request/v1";
const RESPONSE_SCHEMA: &str = "wave-wellen-reader.response/v1";
const MAX_SIGNALS_PER_REQUEST: usize = 64;

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct Request {
    schema: String,
    operation: String,
    path: String,
    project_tick_ps: u64,
    #[serde(default)]
    offset: i64,
    #[serde(default)]
    signal_ids: Vec<String>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct Response {
    schema: &'static str,
    ok: bool,
    format: &'static str,
    #[serde(skip_serializing_if = "Option::is_none")]
    start_tick: Option<i64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    end_tick: Option<i64>,
    signal_count: usize,
    transition_count: u64,
    signals: Vec<SignalResponse>,
    diagnostics: Vec<Diagnostic>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct SignalResponse {
    id: String,
    identifier_code: String,
    scope: String,
    scope_path: Vec<String>,
    reference: String,
    full_name: String,
    width: u32,
    transitions_loaded: bool,
    #[serde(skip_serializing_if = "Vec::is_empty")]
    transitions: Vec<TransitionResponse>,
}

#[derive(Serialize)]
struct TransitionResponse {
    tick: i64,
    value: String,
}

#[derive(Serialize)]
struct Diagnostic {
    severity: &'static str,
    message: String,
}

#[derive(Clone)]
struct SignalDescriptor {
    id: String,
    signal_ref: SignalRef,
    identifier_code: String,
    scope: String,
    scope_path: Vec<String>,
    reference: String,
    full_name: String,
    width: u32,
}

impl SignalDescriptor {
    fn response(
        &self,
        transitions_loaded: bool,
        transitions: Vec<TransitionResponse>,
    ) -> SignalResponse {
        SignalResponse {
            id: self.id.clone(),
            identifier_code: self.identifier_code.clone(),
            scope: self.scope.clone(),
            scope_path: self.scope_path.clone(),
            reference: self.reference.clone(),
            full_name: self.full_name.clone(),
            width: self.width,
            transitions_loaded,
            transitions,
        }
    }
}

fn diagnostic(severity: &'static str, message: impl Into<String>) -> Diagnostic {
    Diagnostic {
        severity,
        message: message.into(),
    }
}

fn error_response(message: impl Into<String>) -> Response {
    Response {
        schema: RESPONSE_SCHEMA,
        ok: false,
        format: "fst",
        start_tick: None,
        end_tick: None,
        signal_count: 0,
        transition_count: 0,
        signals: Vec::new(),
        diagnostics: vec![diagnostic("error", message)],
    }
}

fn checked_pow10(exponent: u32) -> Result<u128, String> {
    let mut value = 1_u128;
    for _ in 0..exponent {
        value = value
            .checked_mul(10)
            .ok_or_else(|| "Waveform timescale exceeds the supported range.".to_string())?;
    }
    Ok(value)
}

fn convert_time(
    raw: u64,
    factor: u32,
    exponent: i8,
    project_tick_ps: u64,
    offset: i64,
) -> Result<i64, String> {
    if project_tick_ps == 0 {
        return Err("Project picoseconds-per-tick must be greater than zero.".to_string());
    }
    let relative_exponent = i16::from(exponent) + 12;
    let numerator = u128::from(raw)
        .checked_mul(u128::from(factor))
        .ok_or_else(|| "Waveform timestamp exceeds the supported range.".to_string())?;
    let numerator = if relative_exponent >= 0 {
        numerator
            .checked_mul(checked_pow10(relative_exponent as u32)?)
            .ok_or_else(|| "Waveform timestamp exceeds the supported range.".to_string())?
    } else {
        numerator
    };
    let denominator = if relative_exponent >= 0 {
        u128::from(project_tick_ps)
    } else {
        checked_pow10((-relative_exponent) as u32)?
            .checked_mul(u128::from(project_tick_ps))
            .ok_or_else(|| "Waveform timescale exceeds the supported range.".to_string())?
    };
    if numerator % denominator != 0 {
        return Err(
            "Waveform timestamp cannot be represented exactly in the project timebase."
                .to_string(),
        );
    }
    let ticks = numerator / denominator;
    if ticks > i64::MAX as u128 {
        return Err("Waveform timestamp exceeds the signed tick range.".to_string());
    }
    (ticks as i64)
        .checked_add(offset)
        .ok_or_else(|| "Aligned waveform timestamp exceeds the signed tick range.".to_string())
}

fn unique_id(base: &str, counts: &mut HashMap<String, usize>) -> String {
    let count = counts.entry(base.to_string()).or_insert(0);
    *count += 1;
    if *count == 1 {
        base.to_string()
    } else {
        format!("{base}#{count}")
    }
}

fn append_var(
    hierarchy: &Hierarchy,
    var_ref: VarRef,
    scope_path: &[String],
    ids: &mut HashMap<String, usize>,
    output: &mut Vec<SignalDescriptor>,
    diagnostics: &mut Vec<Diagnostic>,
) {
    let var = &hierarchy[var_ref];
    if !var.is_bit_vector(hierarchy) {
        diagnostics.push(diagnostic(
            "warning",
            format!(
                "Signal '{}' is not a digital bit vector and was omitted.",
                var.full_name(hierarchy)
            ),
        ));
        return;
    }
    let reference = var.name(hierarchy).to_string();
    let full_name = var.full_name(hierarchy);
    output.push(SignalDescriptor {
        id: unique_id(&full_name, ids),
        signal_ref: var.signal_ref(),
        identifier_code: var.signal_ref().index().to_string(),
        scope: scope_path.join("."),
        scope_path: scope_path.to_vec(),
        reference,
        full_name,
        width: var.length(hierarchy).unwrap_or(1).max(1),
    });
}

fn collect_scope(
    hierarchy: &Hierarchy,
    scope_ref: ScopeRef,
    parent_path: &[String],
    ids: &mut HashMap<String, usize>,
    output: &mut Vec<SignalDescriptor>,
    diagnostics: &mut Vec<Diagnostic>,
) {
    let scope = &hierarchy[scope_ref];
    let mut path = parent_path.to_vec();
    path.push(scope.name(hierarchy).to_string());
    for var_ref in scope.vars(hierarchy) {
        append_var(hierarchy, var_ref, &path, ids, output, diagnostics);
    }
    for child in scope.scopes(hierarchy) {
        collect_scope(hierarchy, child, &path, ids, output, diagnostics);
    }
}

fn describe_signals(
    hierarchy: &Hierarchy,
    diagnostics: &mut Vec<Diagnostic>,
) -> Vec<SignalDescriptor> {
    let mut ids = HashMap::new();
    let mut output = Vec::new();
    for var_ref in hierarchy.vars() {
        append_var(hierarchy, var_ref, &[], &mut ids, &mut output, diagnostics);
    }
    for scope_ref in hierarchy.scopes() {
        collect_scope(
            hierarchy,
            scope_ref,
            &[],
            &mut ids,
            &mut output,
            diagnostics,
        );
    }
    output
}

fn process(request: Request) -> Result<Response, String> {
    if request.schema != REQUEST_SCHEMA {
        return Err(format!(
            "Unsupported request schema '{}'; expected '{REQUEST_SCHEMA}'.",
            request.schema
        ));
    }
    if request.operation != "metadata" && request.operation != "load" {
        return Err(format!("Unsupported operation '{}'.", request.operation));
    }
    if request.operation == "metadata" && !request.signal_ids.is_empty() {
        return Err("Metadata requests must not contain signalIds.".to_string());
    }
    if request.operation == "load" {
        if request.signal_ids.is_empty() {
            return Err("Load requests require at least one signalId.".to_string());
        }
        if request.signal_ids.len() > MAX_SIGNALS_PER_REQUEST {
            return Err(format!(
                "Load request contains {} signals; the maximum is {MAX_SIGNALS_PER_REQUEST}.",
                request.signal_ids.len()
            ));
        }
    }
    if !Path::new(&request.path).is_file() {
        return Err(format!("Waveform file '{}' does not exist.", request.path));
    }

    let mut waveform = wellen::simple::read_with_options(
        &request.path,
        &wellen::LoadOptions {
            multi_thread: true,
            remove_scopes_with_empty_name: false,
        },
    )
    .map_err(|error| format!("Failed to read waveform: {error}"))?;
    if waveform.hierarchy().file_format() != FileFormat::Fst {
        return Err("The Wellen on-demand reader accepts FST input only.".to_string());
    }
    let timescale = waveform
        .hierarchy()
        .timescale()
        .ok_or_else(|| "FST waveform does not declare a timescale.".to_string())?;
    let exponent = timescale
        .unit
        .to_exponent()
        .ok_or_else(|| "FST waveform uses an unknown timescale unit.".to_string())?;
    let convert = |raw| {
        convert_time(
            raw,
            timescale.factor,
            exponent,
            request.project_tick_ps,
            request.offset,
        )
    };
    let start_tick = waveform.time_table().first().copied().map(convert).transpose()?;
    let end_tick = waveform.time_table().last().copied().map(convert).transpose()?;

    let mut diagnostics = Vec::new();
    let descriptors = describe_signals(waveform.hierarchy(), &mut diagnostics);
    if request.operation == "metadata" {
        let signal_count = descriptors.len();
        return Ok(Response {
            schema: RESPONSE_SCHEMA,
            ok: true,
            format: "fst",
            start_tick,
            end_tick,
            signal_count,
            transition_count: 0,
            signals: descriptors
                .iter()
                .map(|signal| signal.response(false, Vec::new()))
                .collect(),
            diagnostics,
        });
    }

    let requested: HashSet<&str> = request.signal_ids.iter().map(String::as_str).collect();
    if requested.len() != request.signal_ids.len() {
        return Err("Load request contains duplicate signalIds.".to_string());
    }
    let selected: Vec<&SignalDescriptor> = descriptors
        .iter()
        .filter(|signal| requested.contains(signal.id.as_str()))
        .collect();
    if selected.len() != requested.len() {
        let available: HashSet<&str> = descriptors.iter().map(|signal| signal.id.as_str()).collect();
        let missing = request
            .signal_ids
            .iter()
            .filter(|id| !available.contains(id.as_str()))
            .cloned()
            .collect::<Vec<_>>()
            .join(", ");
        return Err(format!("Unknown FST signalIds: {missing}."));
    }

    let mut refs = Vec::new();
    let mut seen_refs = HashSet::new();
    for descriptor in &selected {
        if seen_refs.insert(descriptor.signal_ref.index()) {
            refs.push(descriptor.signal_ref);
        }
    }
    waveform.load_signals_multi_threaded(&refs);

    let mut transition_count = 0_u64;
    let mut signals = Vec::with_capacity(selected.len());
    for descriptor in selected {
        let signal = waveform
            .get_signal(descriptor.signal_ref)
            .ok_or_else(|| format!("Wellen did not load signal '{}'.", descriptor.id))?;
        let mut transitions: Vec<TransitionResponse> = Vec::new();
        for (time_index, value) in signal.iter_changes() {
            let raw_time = *waveform
                .time_table()
                .get(time_index as usize)
                .ok_or_else(|| "FST signal references an invalid time-table index.".to_string())?;
            let tick = convert(raw_time)?;
            let rendered = value.to_string();
            if let Some(last) = transitions.last_mut() {
                if last.tick == tick {
                    last.value = rendered;
                    continue;
                }
                if last.value == rendered {
                    continue;
                }
            }
            transitions.push(TransitionResponse {
                tick,
                value: rendered,
            });
        }
        transition_count = transition_count
            .checked_add(transitions.len() as u64)
            .ok_or_else(|| "FST transition count overflow.".to_string())?;
        signals.push(descriptor.response(true, transitions));
    }

    Ok(Response {
        schema: RESPONSE_SCHEMA,
        ok: true,
        format: "fst",
        start_tick,
        end_tick,
        signal_count: descriptors.len(),
        transition_count,
        signals,
        diagnostics,
    })
}

fn main() {
    let mut input = String::new();
    let response = match io::stdin().read_to_string(&mut input) {
        Ok(_) => match serde_json::from_str::<Request>(&input) {
            Ok(request) => process(request).unwrap_or_else(error_response),
            Err(error) => error_response(format!("Invalid JSON request: {error}")),
        },
        Err(error) => error_response(format!("Failed to read request: {error}")),
    };
    match serde_json::to_string(&response) {
        Ok(json) => println!("{json}"),
        Err(error) => println!(
            "{{\"schema\":\"{RESPONSE_SCHEMA}\",\"ok\":false,\"format\":\"fst\",\"signalCount\":0,\"transitionCount\":0,\"signals\":[],\"diagnostics\":[{{\"severity\":\"error\",\"message\":\"JSON serialization failed: {error}\"}}]}}"
        ),
    }
}
