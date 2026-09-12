#pragma once

#include <QAbstractListModel>
#include <QList>

#include "model/vehiclealert.h"

// The list of messages the operator should read, newest at the top.
//
// This is a QAbstractListModel, which is the standard Qt way to hand
// a list to the screen. QML asks it how many rows there are and then
// asks for one named piece of each row at a time. Those named pieces
// are called roles.
class AlertListViewModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int alertCount READ rowCountForQml NOTIFY alertCountChanged)

public:
    // The named pieces of one row. Qt reserves the numbers below
    // Qt::UserRole for itself, so ours start there.
    enum AlertRole
    {
        AlertSeverityValueRole = Qt::UserRole + 1,
        AlertSeverityNameRole,
        AlertTimeTextRole,
        AlertMessageTextRole
    };

    explicit AlertListViewModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int rowCountForQml() const;

public slots:
    // Connected in the composition root to both the radio link and
    // the command panel. The number is an AlertSeverity value.
    void appendAlert(int severityValue, QString messageText);

    void clearAllAlerts();

signals:
    void alertCountChanged();

private:
    QString severityDisplayName(AlertSeverity severity) const;

    QList<VehicleAlert> m_alerts;

    // Old messages fall off the bottom. A ground station left running
    // for hours must not grow forever.
    static constexpr int kMaximumAlertsKept = 200;
};
