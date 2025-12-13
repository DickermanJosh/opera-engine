# Claude + Codex Collaboration Guide


This document describes how Claude can collaborate with OpenAI Codex for autonomous code review and iteration workflows.


## Overview


Codex is an AI code assistant available in this environment that can perform thorough code reviews. Claude can invoke Codex programmatically using its non-interactive `exec` mode, allowing for an autonomous write-review-fix cycle.


## How to Invoke Codex


### Non-Interactive Mode (Required)


Codex requires a TTY for interactive mode, which isn't available when Claude runs commands. Use the `exec` subcommand instead:


```bash
codex exec "Your prompt here" 2>&1
```


**Important:** Always append `2>&1` to capture both stdout and stderr.


### Running in Background


Codex reviews are thorough and can take **5-10 minutes**. Always run in background mode:


```bash
# Run with background: true and timeout: 600000 (10 minutes)
codex exec "Review the uncommitted changes in this repository. Provide a code review with any issues, suggestions, or concerns." 2>&1
```


**CRITICAL: Never kill a Codex process early.** Let it complete its full analysis.


## Code Review Workflow


### Step 1: Initial Review


Ask Codex to review uncommitted changes:


```bash
codex exec "Review the uncommitted changes in this repository. Provide a code review with any issues, suggestions, or concerns." 2>&1
```


For focused reviews, specify the scope:


```bash
codex exec "Review the uncommitted changes in this repository. Focus on the files in [path/to/files]. Provide a code review with any issues, suggestions, or concerns." 2>&1
```


### Step 2: Monitor Progress


Check the background process output periodically using `BashOutput`. Codex will show:
- `thinking` blocks - its analysis process
- `exec` blocks - commands it runs to examine the code
- Final `codex` block - the actual review findings


### Step 3: Analyze Findings


Codex findings typically include:
- **File path and line numbers** - e.g., `mycoolproject/redacted/noleaks.js:26-33`
- **Issue description** - what's wrong
- **Suggested fix** - how to resolve it


### Step 4: Implement Fixes


For each finding:
1. Read the relevant file section
2. Understand the issue
3. Implement the fix
4. Track progress in todo list


### Step 5: Verify Fixes


Run Codex again to verify all issues are resolved:


```bash
codex exec "Review the uncommitted changes in this repository. Focus on [area]. Provide a code review with any issues, suggestions, or concerns." 2>&1
```


### Step 6: Iterate Until Clean


Repeat Steps 3-5 until Codex finds no new bugs. Note that Codex may report:
- **Bugs** - actual issues that need fixing
- **Design suggestions** - valid observations that are intentional design choices


Use judgment to distinguish between bugs requiring fixes and suggestions for future consideration.


## Sample Prompts


### General Code Review
```
Review the uncommitted changes in this repository. Provide a code review with any issues, suggestions, or concerns.
```


### Focused Review
```
Review the uncommitted changes in this repository. Focus on the files in src/components/. Provide a code review with any issues, suggestions, or concerns.
```


### Security Review
```
Review the uncommitted changes for security vulnerabilities. Focus on authentication, input validation, and data handling.
```


### Architecture Review
```
Review the uncommitted changes for architectural concerns. Focus on separation of concerns, dependency management, and scalability.
```


## Best Practices


1. **Be patient** - Codex reviews take 5-10 minutes for thorough analysis
2. **Use todo lists** - Track each finding as a separate task
3. **Fix all bugs first** - Address actual bugs before considering design suggestions
4. **Verify with re-review** - Always run Codex again after making fixes
5. **Document decisions** - Note why design suggestions were accepted or declined
6. **Scope appropriately** - Focus reviews on specific areas when possible for faster iteration


## Interpreting Results


### Bugs (Fix These)
- Wrong file paths or imports
- Logic errors (e.g., array index misalignment)
- Missing error handling that causes crashes
- Security vulnerabilities


### Design Suggestions (Consider These)
- Portability concerns for internal tools
- Alternative approaches that are equally valid
- Intentional design decisions (e.g., empty content not getting vectors)


## Troubleshooting


### "stdout is not a terminal"
Use `codex exec` instead of just `codex`.


### Process seems stuck
Codex reviews are thorough. Wait the full 5-10 minutes before considering intervention.


### No findings returned
Check that there are actually uncommitted changes (`git status`).


### Codex can't find files
Ensure you're running from the correct working directory.


## Example Session


```
1. User: "Review my changes"
2. Claude: Runs `codex exec "Review uncommitted changes..." 2>&1` in background
3. Claude: Monitors with BashOutput until complete
4. Codex: Reports 4 issues with file:line references
5. Claude: Creates todo list with 4 items
6. Claude: Fixes each issue, marking todos complete
7. Claude: Runs Codex review again
8. Codex: Reports no new bugs (only design suggestions)
9. Claude: Reports success to user
```


## Integration with Claude Workflow


This collaboration works best when:
- User requests a code review or asks Claude to fix issues
- Claude needs a "second opinion" on complex changes
- Validating that fixes are complete before committing
- Catching bugs that might be missed in manual review


The autonomous cycle (write → review → fix → verify) can run without user intervention, making it ideal for thorough code quality assurance.
