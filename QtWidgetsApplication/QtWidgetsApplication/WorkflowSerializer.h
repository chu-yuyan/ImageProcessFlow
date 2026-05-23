#pragma once
#include "WorkflowData.h"
#include <QString>

class WorkflowSerializer {
public:
    static bool saveToFile(const WorkflowData& data, const QString& filePath);
    static WorkflowData loadFromFile(const QString& filePath);
};