#include "viewmodel/alertlistviewmodel.h"

#include <QDateTime>

AlertListViewModel::AlertListViewModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AlertListViewModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;  // this is a flat list, so child rows do not exist
    return static_cast<int>(m_alerts.size());
}

int AlertListViewModel::rowCountForQml() const
{
    return static_cast<int>(m_alerts.size());
}

QVariant AlertListViewModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_alerts.size())
        return QVariant();

    const VehicleAlert &oneAlert = m_alerts.at(index.row());

    switch (role) {
    case AlertSeverityValueRole: return static_cast<int>(oneAlert.severity);
    case AlertSeverityNameRole:  return severityDisplayName(oneAlert.severity);
    case AlertTimeTextRole:      return oneAlert.timeText;
    case AlertMessageTextRole:   return oneAlert.messageText;
    default:                     return QVariant();
    }
}

QHash<int, QByteArray> AlertListViewModel::roleNames() const
{
    // These are the names QML uses inside a delegate. The text on the
    // left of the arrow is the number, the text on the right is what
    // QML types.
    QHash<int, QByteArray> namesForQml;
    namesForQml[AlertSeverityValueRole] = "alertSeverityValue";
    namesForQml[AlertSeverityNameRole]  = "alertSeverityName";
    namesForQml[AlertTimeTextRole]      = "alertTimeText";
    namesForQml[AlertMessageTextRole]   = "alertMessageText";
    return namesForQml;
}

void AlertListViewModel::appendAlert(int severityValue, QString messageText)
{
    VehicleAlert newAlert;
    newAlert.severity = static_cast<AlertSeverity>(severityValue);
    newAlert.messageText = messageText;
    newAlert.timeText = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"));

    // Newest on top, so the operator reads down from the most recent.
    // beginInsertRows and endInsertRows are how a list model tells
    // the screen a row is arriving. Skip them and QML draws the old
    // list against the new data and crashes.
    beginInsertRows(QModelIndex(), 0, 0);
    m_alerts.prepend(newAlert);
    endInsertRows();

    if (m_alerts.size() > kMaximumAlertsKept) {
        const int lastRow = static_cast<int>(m_alerts.size()) - 1;
        beginRemoveRows(QModelIndex(), lastRow, lastRow);
        m_alerts.removeLast();
        endRemoveRows();
    }

    emit alertCountChanged();
}

void AlertListViewModel::clearAllAlerts()
{
    beginResetModel();
    m_alerts.clear();
    endResetModel();
    emit alertCountChanged();
}

QString AlertListViewModel::severityDisplayName(AlertSeverity severity) const
{
    switch (severity) {
    case AlertSeverity::Information: return QStringLiteral("INFO");
    case AlertSeverity::Warning:     return QStringLiteral("WARN");
    case AlertSeverity::Critical:    return QStringLiteral("CRIT");
    }
    return QStringLiteral("INFO");
}
