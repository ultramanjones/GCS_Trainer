#pragma once

#include <QString>

// How bad an alert is. Plain numbers so nothing above the model layer
// has to know about a Qt class to read them.
enum class AlertSeverity : int
{
    Information = 0,
    Warning = 1,
    Critical = 2
};

// One line in the alert list. A plain data holder, nothing more.
struct VehicleAlert
{
    AlertSeverity severity = AlertSeverity::Information;
    QString messageText;

    // Wall clock time the alert happened, already turned into text
    // for the screen. Kept as text so no layer above has to format it.
    QString timeText;
};
