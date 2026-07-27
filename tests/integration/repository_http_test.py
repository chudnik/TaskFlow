#!/usr/bin/env python3

import json
import sys
import time
import urllib.error
import urllib.parse
import urllib.request


BASE_URL = sys.argv[1].rstrip("/")
RUN_ID = str(time.time_ns())


def request(method, path, body=None, token=None, expected=(200,), request_id=None):
    headers = {"X-Request-ID": request_id or f"repository-http-{RUN_ID}"}
    if body is not None:
        headers["Content-Type"] = "application/json"
    if token:
        headers["Authorization"] = f"Bearer {token}"
    encoded = json.dumps(body).encode() if body is not None else None
    outgoing = urllib.request.Request(
        BASE_URL + path, data=encoded, headers=headers, method=method
    )
    try:
        with urllib.request.urlopen(outgoing, timeout=5) as response:
            payload = json.loads(response.read() or b"{}")
            status = response.status
            response_headers = response.headers
    except urllib.error.HTTPError as error:
        payload = json.loads(error.read() or b"{}")
        status = error.code
        response_headers = error.headers
    assert status in expected, (method, path, status, payload)
    assert response_headers["X-Request-ID"] == headers["X-Request-ID"]
    return payload


owner = request(
    "POST",
    "/api/v1/auth/register",
    {"email": f"owner-{RUN_ID}@example.test", "password": "correct horse battery staple"},
    expected=(201,),
)
member = request(
    "POST",
    "/api/v1/auth/register",
    {"email": f"member-{RUN_ID}@example.test", "password": "correct horse battery staple"},
    expected=(201,),
)
owner_token = owner["access_token"]
member_token = member["access_token"]

request("GET", "/api/v1/projects", expected=(401,))
project = request(
    "POST", "/api/v1/projects", {"name": "HTTP integration", "description": RUN_ID},
    owner_token, (201,)
)
project_id = project["id"]
request(
    "POST",
    f"/api/v1/projects/{project_id}/members",
    {"user_id": member["user"]["id"], "role": "member"},
    owner_token,
    (201,),
)

task = request(
    "POST",
    "/api/v1/tasks",
    {
        "project_id": project_id,
        "title": "Repository-backed task",
        "assignee_id": member["user"]["id"],
        "deadline_at": "2030-01-01T12:00:00Z",
    },
    owner_token,
    (201,),
)
task_id = task["id"]
request(
    "PUT",
    f"/api/v1/tasks/{task_id}",
    {
        "title": "stale",
        "description": "",
        "priority": "medium",
        "version": 0,
        "deadline_at": "2030-01-01T12:00:00Z",
    },
    owner_token,
    (409,),
)
comment = request(
    "POST",
    f"/api/v1/tasks/{task_id}/comments",
    {"body": "Persistent comment"},
    member_token,
    (201,),
)
request("GET", f"/api/v1/tasks/{task_id}/comments", token=owner_token, expected=(200,))
request("GET", f"/api/v1/projects/{project_id}/history", token=owner_token, expected=(200,))
request("GET", f"/api/v1/tasks/{task_id}/history", token=member_token, expected=(200,))

query = urllib.parse.urlencode({"project_id": project_id, "page_size": 100})
before = request("GET", f"/api/v1/tasks?{query}", token=owner_token)["items"]
request(
    "POST",
    "/api/v1/tasks",
    {"project_id": project_id, "title": ""},
    owner_token,
    (422,),
)
after = request("GET", f"/api/v1/tasks?{query}", token=owner_token)["items"]
assert [item["id"] for item in before] == [item["id"] for item in after]
assert comment["task_id"] == task_id

print("repository-backed HTTP scenario passed")
