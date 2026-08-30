#ifndef APPSTYLE_H
#define APPSTYLE_H

#include <QColor>
#include <QIcon>

class QApplication;
class QComboBox;
class QWidget;

namespace AppStyle {
void apply(QApplication &app);
QComboBox *createComboBox(QWidget *parent = nullptr);
}

namespace AppIcons {
QIcon record(const QColor &color = QColor(QStringLiteral("#E11D2E")));
QIcon pause(const QColor &color = QColor(QStringLiteral("#FFFFFF")));
QIcon resume(const QColor &color = QColor(QStringLiteral("#FFFFFF")));
QIcon stop(const QColor &color = QColor(QStringLiteral("#FFFFFF")));
QIcon mic(const QColor &color = QColor(QStringLiteral("#1B2838")));
QIcon speaker(const QColor &color = QColor(QStringLiteral("#1B2838")));
QIcon settings(const QColor &color = QColor(QStringLiteral("#1B2838")));
QIcon back(const QColor &color = QColor(QStringLiteral("#1B2838")));
QIcon folder(const QColor &color = QColor(QStringLiteral("#1B2838")));
QIcon chevronDown(const QColor &color = QColor(QStringLiteral("#6B7280")));
QIcon chevronRight(const QColor &color = QColor(QStringLiteral("#6B7280")));
}

#endif
