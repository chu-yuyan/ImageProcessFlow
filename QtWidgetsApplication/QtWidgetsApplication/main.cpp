#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include "QtWidgetsApplication.h"
#include "WorkflowSerializer.h"
#include "WorkflowExecutor.h"
#include <QtWidgets/QApplication>

int main(int argc, char* argv[])
{
    // 先解析命令行，判断是否使用 --no-gui
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption(QCommandLineOption("no-gui", "Run without GUI"));
    parser.addOption(QCommandLineOption("workflow", "Workflow JSON file", "filepath"));

    bool noGui = false;
    QString workflowPath;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--no-gui") == 0) noGui = true;
        else if (strcmp(argv[i], "--workflow") == 0 && i + 1 < argc) {
            workflowPath = argv[++i];
        }
    }

    if (noGui && !workflowPath.isEmpty()) {
        // 命令行模式：使用 QCoreApplication
        QCoreApplication app(argc, argv);
        WorkflowData data = WorkflowSerializer::loadFromFile(workflowPath);
        if (data.nodes.isEmpty()) {
            qDebug() << "Failed to load workflow file:" << workflowPath;
            return 1;
        }
        bool success = WorkflowExecutor::execute(data);
        return success ? 0 : 1;
    }
    else {
        // 正常 GUI 模式
        QApplication app(argc, argv);
        QtWidgetsApplication window;
        window.show();
        return app.exec();
    }
}