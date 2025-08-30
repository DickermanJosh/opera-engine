# Branch Protection Setup Guide

This guide explains how to configure GitHub branch protection rules to ensure all tests pass before merging to main.

## Required Branch Protection Rules

### For `main` branch:

1. **Go to Repository Settings** → **Branches** → **Add rule**

2. **Branch name pattern**: `main`

3. **Enable the following protection rules**:

#### ✅ **Require pull request reviews before merging**
- Required approving reviews: `1`
- Dismiss stale PR approvals when new commits are pushed: `✓`
- Require review from code owners: `✓` (if CODEOWNERS file exists)

#### ✅ **Require status checks to pass before merging**
- Require branches to be up to date before merging: `✓`

**Required status checks** (add these manually):
```
Build and Test (ubuntu-latest, Debug)
Build and Test (ubuntu-latest, Release)  
Build and Test (macos-latest, Debug)
Build and Test (macos-latest, Release)
Build and Test (windows-latest, Debug)
Build and Test (windows-latest, Release)
Code Quality Checks
Security Scan  
Performance Benchmarks
Validate Project Structure
```

#### ✅ **Require linear history**
- Enforce a linear history by requiring deployments to succeed before merging: `✓`

#### ✅ **Include administrators**
- Include administrators: `✓`
- Allow force pushes: `✗`
- Allow deletions: `✗`

## Additional Protection (Optional but Recommended)

### For `develop` branch:
- Same rules as main, but with fewer required status checks
- Only require: Build and Test + Code Quality Checks

### Repository Settings:
1. **Actions** → **General** → **Fork pull request workflows**: 
   - "Require approval for first-time contributors"
   - "Require approval for all outside collaborators"

2. **Security** → **Code security and analysis**:
   - Enable Dependabot alerts
   - Enable Dependabot security updates
   - Enable CodeQL analysis

## Workflow Status Checks

The CI workflow creates these status checks that must pass:

| Status Check | Purpose | Required for Main |
|--------------|---------|-------------------|
| `Build and Test (ubuntu-latest, Debug)` | Linux Debug build + all 20 tests | ✅ |
| `Build and Test (ubuntu-latest, Release)` | Linux Release build + all 20 tests | ✅ |
| `Build and Test (macos-latest, Debug)` | macOS Debug build + all 20 tests | ✅ |
| `Build and Test (macos-latest, Release)` | macOS Release build + all 20 tests | ✅ |
| `Build and Test (windows-latest, Debug)` | Windows Debug build + all 20 tests | ✅ |
| `Build and Test (windows-latest, Release)` | Windows Release build + all 20 tests | ✅ |
| `Code Quality Checks` | Formatting, linting, static analysis | ✅ |
| `Security Scan` | CodeQL security analysis | ✅ |
| `Performance Benchmarks` | Performance regression testing | ✅ |
| `Validate Project Structure` | Project structure validation | ✅ |

## Testing the Protection

### ✅ **Valid PR Scenario**:
1. Create feature branch: `git checkout -b feature/new-feature`
2. Make changes, commit, push
3. Create PR to `main`
4. All status checks pass (green ✓)
5. PR can be merged

### ❌ **Invalid PR Scenario**:
1. Create branch with failing test
2. Push changes that break build or tests  
3. Create PR to `main`
4. Status checks fail (red ✗)
5. **PR cannot be merged until fixed**

## Emergency Procedures

### If CI is broken:
1. **Never disable branch protection** - instead fix CI
2. Use emergency fix process:
   ```bash
   git checkout -b hotfix/emergency-fix
   # Fix the issue
   git commit -m "hotfix: emergency fix for critical issue"  
   git push origin hotfix/emergency-fix
   # Create PR - all checks must still pass
   ```

### For repository administrators:
- Even with admin access, follow the same PR process
- Branch protection applies to everyone for consistency
- Use "Bypass pull request requirements" only for genuine emergencies

## Verification Commands

After setup, verify protection is working:

```bash
# Check protection status
curl -H "Authorization: token YOUR_TOKEN" \
  https://api.github.com/repos/OWNER/REPO/branches/main/protection

# Test a failing PR (should be blocked)
git checkout -b test/failing-branch
echo "broken code" >> cpp/src/board/Board.cpp  
git commit -am "test: intentionally broken commit"
git push origin test/failing-branch
# Create PR - should show failing checks and block merge
```

## Monitoring

- Check **Insights** → **Pulse** for PR merge patterns
- Monitor **Actions** tab for workflow success rates  
- Review **Security** tab for any security alerts

This setup ensures the main branch always contains working, tested code that passes all quality gates.