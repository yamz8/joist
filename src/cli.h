#pragma once

#include <QStringList>

// Commands that need no display, for scripts, timers, and coding agents.
// Each exits 0 or 1, with errors on stderr; most take --json.
bool isCliCommand(const QString &word);
int runCli(const QStringList &arguments);
QString cliSummary();
