#pragma once

#include <QJsonDocument>
#include <QJsonObject>

namespace wave {

// The application and CLI share this public description; SuiteApp transports it.
inline QJsonObject describeSessionAutomation()
{
    return QJsonDocument::fromJson(R"json({
      "schema": "wave-workbench.session/v1",
      "transport": "suite-app/v1",
      "minimumSdkVersion": "1.0.2",
      "argumentsSchemaRef": "schemas/automation/v1/session-request.schema.json",
      "responseSchemaRef": "schemas/automation/v1/session-response.schema.json",
      "providerPrefix": "wave.session.",
      "commands": ["list","status","open","refresh","view","apply","save","undo","redo"],
      "sessionRequired": true,
      "maxInputBytes": 524288,
      "maxSelectionIds": 1000,
      "maxOperations": 4096,
      "maxWaitMs": 30000,
      "timeFormat": "decimal tick strings or existing physical time expressions, e.g. 20 ns",
      "revisions": {
        "modelSha256": "SHA-256 of the serialized in-memory project; required for apply/save/undo/redo",
        "loadedSha256": "SHA-256 of the exact file bytes adopted by this window",
        "diskSha256": "SHA-256 of current disk bytes, or null",
        "expectedDiskSha256": "optional for open/refresh; required for save; 'missing' permits a new file only"
      },
      "arguments": {
        "status": [],
        "open": ["projectPath","expectedDiskSha256"],
        "refresh": ["expectedDiskSha256"],
        "view": ["scenarioId","cursor","start","end","zoom","pan","laneIds","range","markerIds","relationIds","groups","relationsVisible","dryRun"],
        "apply": ["batch","scenarioId","expectedModelSha256","dryRun"],
        "save": ["projectPath","expectedModelSha256","expectedDiskSha256","dryRun"],
        "undo": ["expectedModelSha256","dryRun"],
        "redo": ["expectedModelSha256","dryRun"]
      },
      "view": {
        "zoom": ["in","out","fit"],
        "range": {"start":"time","end":"time"},
        "clearRange": null,
        "groups": [{"id":"stable group ID","collapsed":true}],
        "visibility": "Persisted signal/group visibility uses apply update-signal/update-group visible.",
        "viewport": "start/end are requested bounds; read back actual pixel-quantized visibleStart/visibleEnd.",
        "selection": "laneIds selects visible signals; group IDs expand to their visible member signals.",
        "atomicValidation": true
      },
      "exitCodes": {"success":0,"usage":2,"inputOrTransport":3,"rejected":4,"output":5,"adoptionPending":6},
      "notes": [
        "All session responses are JSON on stdout, diagnostics on stderr.",
        "list discovers every window registered at --runtime; all other commands require --session.",
        "Dirty or active editors block open/adoption; online edits require the current model revision.",
        "apply shares offline business validation and commits one undoable transaction.",
        "No command discards unsaved changes or overwrites an external revision.",
        "Relative CLI paths resolve against the caller working directory.",
        "A timed-out mutating request has unknown outcome: inspect status before retrying."
      ]
    })json").object();
}

} // namespace wave
