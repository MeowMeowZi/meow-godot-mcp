---
name: release
description: "Use this skill when the user wants to create a release, publish a version, push a tag, build and package for all platforms, or sync releases to Gitee. Triggers: 'release', 'publish', 'tag', 'package', 'build and release', 'sync to gitee', 'create version'. This skill handles the full workflow: commit, push, tag, trigger GitHub Actions CI/CD, wait for builds, create GitHub Release with Chinese description, upload platform-specific packages, and sync everything to Gitee."
---

# Release Workflow Skill

## Overview

Automates the full release pipeline for the Godot MCP Meow project:
1. Commit and push code
2. Create and push a version tag
3. Trigger GitHub Actions workflow (3-platform build + tests)
4. Wait for CI completion
5. Create/update GitHub Release with Chinese description and download table
6. Download platform artifacts and upload as individual release assets
7. Create matching Gitee Release with same description and packages

## Prerequisites

- Tokens stored in `.env.local` at project root:
  - `GITHUB_TOKEN` — GitHub personal access token (repo + workflow scope)
  - `GITHUB_REPO` — e.g. `MeowMeowZi/meow-godot-mcp`
  - `GITEE_TOKEN` — Gitee personal access token
  - `GITEE_REPO` — e.g. `MeowMeowZi/meow-godot-mcp`
- Git remotes: `github` (GitHub), `origin` (Gitee)
- GitHub Actions workflow at `.github/workflows/builds.yml` triggered by `v*` tags
- `gh` CLI has proxy issues on this machine — use `curl --noproxy '*'` with REST APIs

## Step-by-Step Procedure

### Step 1: Determine version

Ask the user for the version tag if not provided (e.g. `v1.4.0`). Store as `$VERSION`.

### Step 2: Commit pending changes

```bash
git add <relevant files>
git commit -m "feat: $VERSION release description"
```

Only commit if there are uncommitted changes. Skip if working tree is clean.

### Step 3: Push to both remotes

```bash
git push github master
git push origin master
```

### Step 4: Create and push tag

```bash
git tag $VERSION
git push github $VERSION
git push origin $VERSION
```

If tag already exists and needs to be moved:
```bash
git tag -d $VERSION
git tag $VERSION
git push github :refs/tags/$VERSION && git push github $VERSION
git push origin :refs/tags/$VERSION && git push origin $VERSION
```

### Step 5: Wait for GitHub Actions workflow

Load tokens:
```bash
source .env.local
```

Poll until complete (timeout 15 minutes):
```bash
# Get latest run
RUN_ID=$(curl --noproxy '*' -s -H "Authorization: token $GITHUB_TOKEN" \
  "https://api.github.com/repos/$GITHUB_REPO/actions/runs?per_page=1" | \
  python -c "import sys,json; print(json.load(sys.stdin)['workflow_runs'][0]['id'])")

# Poll status every 15s
STATUS=$(curl --noproxy '*' -s -H "Authorization: token $GITHUB_TOKEN" \
  "https://api.github.com/repos/$GITHUB_REPO/actions/runs/$RUN_ID" | \
  python -c "import sys,json; r=json.load(sys.stdin); print(r['status'], r.get('conclusion',''))")
```

Check job results:
```bash
curl --noproxy '*' -s -H "Authorization: token $GITHUB_TOKEN" \
  "https://api.github.com/repos/$GITHUB_REPO/actions/runs/$RUN_ID/jobs" | \
  python -c "import sys,json; [print(f'{j[\"name\"]}: {j[\"conclusion\"]}') for j in json.load(sys.stdin)['jobs']]"
```

If failed: check which job failed, show error, abort.

### Step 6: Create/update GitHub Release with Chinese description

The workflow auto-creates the release with a combined zip. Update it with Chinese description:

```bash
RELEASE_ID=$(curl --noproxy '*' -s -H "Authorization: token $GITHUB_TOKEN" \
  "https://api.github.com/repos/$GITHUB_REPO/releases/tags/$VERSION" | \
  python -c "import sys,json; print(json.load(sys.stdin)['id'])")
```

Build Chinese release body with:
- Version title in Chinese
- Feature list in Chinese
- Download table (全平台合并包, 仅 Windows, 仅 Linux, macOS Universal)
- Test info

Update via PATCH:
```bash
curl --noproxy '*' -s -X PATCH \
  -H "Authorization: token $GITHUB_TOKEN" \
  -H "Content-Type: application/json" \
  --data-raw '{"name": "$VERSION - Chinese title", "body": "Chinese body"}' \
  "https://api.github.com/repos/$GITHUB_REPO/releases/$RELEASE_ID"
```

### Step 7: Upload platform-specific packages

Download per-platform artifacts from the workflow run:
```bash
# List artifacts
curl --noproxy '*' -s -H "Authorization: token $GITHUB_TOKEN" \
  "https://api.github.com/repos/$GITHUB_REPO/actions/runs/$RUN_ID/artifacts"

# Download each artifact
curl --noproxy '*' -s -L -H "Authorization: token $GITHUB_TOKEN" \
  -H "Accept: application/octet-stream" \
  "https://api.github.com/repos/$GITHUB_REPO/actions/artifacts/$ART_ID/zip" \
  -o "$FILENAME"
```

Upload to release as individual assets:
```bash
curl --noproxy '*' -s -X POST \
  -H "Authorization: token $GITHUB_TOKEN" \
  -H "Content-Type: application/zip" \
  --data-binary "@$FILE" \
  "https://uploads.github.com/repos/$GITHUB_REPO/releases/$RELEASE_ID/assets?name=$NAME"
```

Name convention: `$VERSION-linux-x86_64.zip`, `$VERSION-macos-universal.zip`, `$VERSION-windows-x86_64.zip`

### Step 8: Clean up stale release assets

Delete any assets from old versions that may have been carried over:
```bash
curl --noproxy '*' -s -X DELETE \
  -H "Authorization: token $GITHUB_TOKEN" \
  "https://api.github.com/repos/$GITHUB_REPO/releases/assets/$ASSET_ID"
```

### Step 9: Create Gitee Release with same description and packages

Create release (use Python for Chinese encoding):
```python
import urllib.request, json
data = json.dumps({
    'access_token': GITEE_TOKEN,
    'tag_name': VERSION,
    'name': 'Chinese title',
    'body': 'Chinese body (same as GitHub)',
    'target_commitish': 'master'
}).encode('utf-8')
req = urllib.request.Request(
    f'https://gitee.com/api/v5/repos/{GITEE_REPO}/releases',
    data=data,
    headers={'Content-Type': 'application/json; charset=utf-8'},
    method='POST'
)
resp = urllib.request.urlopen(req)
release_id = json.loads(resp.read())['id']
```

Upload packages to Gitee:
```bash
curl --noproxy '*' -s -X POST \
  "https://gitee.com/api/v5/repos/$GITEE_REPO/releases/$GITEE_RELEASE_ID/attach_files" \
  -F "access_token=$GITEE_TOKEN" \
  -F "file=@$FILE"
```

### Step 10: Report

Print summary with links to both releases:
- GitHub: `https://github.com/$GITHUB_REPO/releases/tag/$VERSION`
- Gitee: `https://gitee.com/$GITEE_REPO/releases/tag/$VERSION`

## Important Notes

- `gh` CLI does NOT work on this machine (proxy issues) — always use `curl --noproxy '*'`
- Gitee API JSON body with Chinese MUST use Python `urllib` (not curl) for correct UTF-8 encoding
- Gitee `create release` API requires `target_commitish` field
- GitHub Actions `release` job requires `startsWith(github.ref, 'refs/tags/v')` — only tag pushes trigger it
- Build target for local testing is `scons target=template_debug` (NOT `target=editor`) per `.gdextension` config
- Workflow artifacts are separate from release assets — artifacts must be downloaded then re-uploaded as release assets
