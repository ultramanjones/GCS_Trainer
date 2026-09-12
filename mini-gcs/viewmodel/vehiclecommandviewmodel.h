#pragma once

#include <QObject>
#include <QString>

class GroundControlStation;

// The buttons that send orders, and what came back.
//
// This object sends and displays. It does not decide. Whether an order
// may be sent, how long to wait for an answer, and what to do when
// none comes are all the ground station's business one layer down.
//
// No button is ever grayed out. A guardrail advises and never decides,
// so every button always sends, the vehicle answers yes or no, and the
// answer is printed. The operator is never left poking a dead button.
class VehicleCommandViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString lastCommandResultText READ lastCommandResultText NOTIFY lastCommandResultTextChanged)

public:
    explicit VehicleCommandViewModel(GroundControlStation *groundControlStation,
                                     QObject *parent = nullptr);

    QString lastCommandResultText() const;

    Q_INVOKABLE void requestArm();
    Q_INVOKABLE void requestDisarm();
    Q_INVOKABLE void requestLaunch();
    Q_INVOKABLE void requestReturnToHome();
    Q_INVOKABLE void requestLand();

    // Cut the motors, right now, wherever the aircraft is. The ground
    // station never holds this one back.
    Q_INVOKABLE void requestEmergencyStop();

    // These two do not go to the vehicle. They pull the radio plug and
    // put it back, so the out of contact behavior can be watched on
    // purpose.
    Q_INVOKABLE void requestRadioUnplug();
    Q_INVOKABLE void requestRadioReconnect();

public slots:
    void showCommandOutcome(QString commandName, bool wasAccepted, QString reason);

signals:
    void lastCommandResultTextChanged();

private:
    void setLastCommandResultText(const QString &resultText);

    GroundControlStation *m_groundControlStation = nullptr;
    QString m_lastCommandResultText;
};
