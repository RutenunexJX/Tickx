"""Extract words and bounds from actual PDF pages for the Qt export regression."""
import json
import sys
import pdfplumber

with pdfplumber.open(sys.argv[1]) as document:
    print(json.dumps([
        {"width": page.width, "height": page.height, "words": page.extract_words()}
        for page in document.pages
    ]))
