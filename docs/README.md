# Documentation

The public documentation website is built from this directory with [ESP-Docs](https://docs.espressif.com/projects/esp-docs/en/latest/), an Espressif wrapper around Sphinx.

## Build locally

From the repository root:

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r docs/requirements.txt
cd docs
build-docs -l en build
```

The generated English HTML is written to `_build/en/generic/html`. Preview it
from the `docs` directory:

```bash
python3 -m http.server 8000 --directory _build/en/generic/html
```

Then open <http://localhost:8000/>.

The site uses ESP-Docs' generic mode: it has no chip target selector. Shared
procedures belong in `en/`; application-specific instructions stay with the
corresponding example under `../examples/`.
