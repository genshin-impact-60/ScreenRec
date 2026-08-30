#ifndef AUDIOCHIP_H
#define AUDIOCHIP_H

#include <QCheckBox>
#include <QElapsedTimer>

class AudioChip : public QCheckBox
{
    Q_OBJECT

public:
    explicit AudioChip(const QString &text, QWidget *parent = nullptr);

    void setLevel(qreal level);
    void setIdleTip(const QString &text);
    void setSilentHint(const QString &text);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void setSilent(bool silent);

    qreal m_level = 0;
    QString m_baseTip;
    QString m_silentHint;
    QElapsedTimer m_quietSince;
    bool m_silent = false;
};

#endif
