#pragma once

#include <QtCore/QObject>
#include <QtCore/QVector>

class QMainWindow;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace prodbg {

class View;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class ViewHandler : public QObject {
    Q_OBJECT;

   public:
    ViewHandler(QObject* parent);
    ~ViewHandler();

    void addView(View* view);
    void readSettings(QMainWindow* mainWindow);

    // Q_SLOT void closeView(QObject* object);

   private:
    void writeSettings();

    QVector<View*> m_views;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

}  // namespace prodbg
