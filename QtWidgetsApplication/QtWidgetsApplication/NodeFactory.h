#pragma once
#include <memory>
#include <QString>
#include <QMap>
#include <functional>
#include "BaseNode.h"

class NodeFactory 
{
public:
    using Creator = std::function<std::unique_ptr<BaseNode>()>;
    
    static NodeFactory& instance() 
    {
        static NodeFactory factory;
        return factory;
    }
    void registerNode(const QString& typeName, Creator creator) 
    {
        m_creators[typeName] = creator;
    }

    std::unique_ptr<BaseNode> create(const QString& typeName) 
    {
        auto it = m_creators.find(typeName);
        if (it != m_creators.end())
            return it.value()();
        return nullptr;
    }
private:
    NodeFactory() = default;
    QMap<QString, Creator> m_creators;
};