#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

// The buttons that send orders to the vehicle, and what came back.
//
// This object does not hold a pointer to the radio link. It raises a
// signal saying what the operator asked for, and the composition root
// connects that signal to the link with a queued connection. Keeping
// the thread hop in one place means no other file has to think about
// threads at all.
//
// Every order starts a clock. If the vehicle does not answer before
// the clock runs out, that is reported as a failure. A ground station
// that sends an order and never checks for the answer is lying to the
// operator.
class CommandPanelViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString pendingCommandName READ pendingCommandName NOTIFY pendingCommandNameChanged)
    Q_PROPERTY(bool isWaitingForAcknowledgment READ isWaitingForAcknowledgment NOTIFY pendingCommandNameChanged)
    Q_PROPERTY(QString lastCommandResultText READ lastCommandResultText NOTIFY lastCommandResultTextChanged)

public:
    explicit CommandPanelViewModel(QObject *parent = nullptr);

    QString pendingCommandName() const;
    bool isWaitingForAcknowledgment() const;
    QString lastCommandResultText() const;

    Q_INVOKABLE void requestArm();
    Q_INVOKABLE void requestDisarm();
    Q_INVOKABLE void requestLaunch();
    Q_INVOKABLE void requestReturnToLaunch();
    Q_INVOKABLE void requestLand();

    // These two do not go to the vehicle. They pull the radio plug
    // and put it back, so the stale-link behavior can be watched on
    // purpose.
    Q_INVOKABLE void requestDropLink();
    Q_INVOKABLE void requestReconnectLink();

public slots:
    // Connected in the composition root to the radio link, with a
    // queued connection.
    void applyCommandAcknowledgment(QString commandName, bool wasAccepted);

signals:
    void commandRequested(QString commandName);
    void dropLinkRequested();
    void reconnectLinkRequested();

    // Raised so the composition root can put it in the alert list.
    // This object never talks to the alert list itself. Two view
    // models reaching sideways to each other is the start of a mess.
    void operatorMessageRaised(int severityValue, QString messageText);

    void pendingCommandNameChanged();
    void lastCommandResultTextChanged();

private slots:
    void handleAcknowledgmentTimeout();

private:
    void sendCommandAndStartClock(const QString &commandName);
    void setPendingCommandName(const QString &commandName);
    void setLastCommandResultText(const QString &resultText);

    QString m_pendingCommandName;
    QString m_lastCommandResultText;
    QTimer m_acknowledgmentTimeoutTimer;

    static constexpr int kAcknowledgmentTimeoutMilliseconds = 2000;
};
