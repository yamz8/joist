---
name: joist
description: Keep RFI and submittal logs for construction projects with the joist command — log items, pass the ball between parties, close them, link Lintel drawing sheets, and export logs and forms as PDF. Use when asked about RFIs, submittals, ball in court, overdue items, or a project's logs.
---

# Joist

Joist keeps a project's RFIs and submittals. Every open item is held by one
party on the project team (the ball is in their court) and has a due date.
Projects are JSON files in `~/.local/share/joist/projects/`; change them with
the commands below, not by editing the files, and the open window reloads.

Every command exits 0 or 1 with the error on stderr, and takes `--json`.
Name the project with `-p` (its name, number, or file) when there is more
than one. Items are written `RFI-3` or `SUB-12`.

```sh
joist projects --json
joist new "Harbor Clinic" -n 2417 --party "Contractor" --party "Architect" --party "Owner"
joist list -p 2417 --open --json            # also --overdue, --type rfi, --holder Architect
joist show RFI-3 -p 2417 --json             # with history and linked sheets
joist add rfi "Slab edge at grid C" -p 2417 -q "Detail 5/S-301 conflicts with..." --spec "03 30 00"
joist add submittal "Rebar shop drawings" -p 2417 --to Owner --due-in 10
joist pass RFI-3 Architect -p 2417 --due-in 5 --note "Needs the structural answer first"
joist close RFI-3 answered -p 2417 --answer "Use 5/S-302."
joist close SUB-4 revise-resubmit -p 2417 --note "Lap lengths"   # back to the submitter as revision 1
joist reopen RFI-3 -p 2417
joist link RFI-3 drawings.pdf A-101 -p 2417  # lintel sheets drawings.pdf lists the sheets
joist export rfi -p 2417 -o rfi-log.pdf      # also submittal, board (who holds what), or RFI-3 for its form
joist notify --dry-run --json                # overdue items not yet announced today
```

Closing actions: an RFI is `answered` or `withdrawn`; a submittal is
`approved`, `approved-as-noted`, `rejected`, or `withdrawn`, and
`revise-resubmit` keeps it open as its next revision with whoever submitted
it. `pass` and `add` take parties as the team spells them (any case);
`joist projects --json` lists each project's team.
