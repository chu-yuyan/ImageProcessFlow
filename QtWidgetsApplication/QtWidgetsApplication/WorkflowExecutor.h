#pragma once
#include "WorkflowData.h"
#include <QList>
#include <memory>
class BaseNode;

class WorkflowExecutor {
public:
    // 执行工作流，返回是否成功
    static bool execute(const WorkflowData& data);
};