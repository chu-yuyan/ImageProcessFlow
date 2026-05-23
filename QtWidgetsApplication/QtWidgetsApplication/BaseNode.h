#pragma once
#include <QString>
#include <QVariant>
#include <QMap>
#include <QList>
#include <opencv2/opencv.hpp>

enum class ImageType { Color, Gray, Pixel };

struct Port
{
    QString name;
    ImageType type;
    QVariant data;
};

class BaseNode
{
protected:
    QString m_name;

    QList<Port> m_inputs;
    QList<Port> m_outputs;

public:
    virtual ~BaseNode() = default;
    virtual void process() = 0;
    virtual QString typeName() const = 0;

    virtual QMap<QString, QVariant> getParameters() const { return {}; }
    virtual void setParameter(const QString& name, const QVariant& value) {}

    // 端口访问
    QList<Port>& inputs() { return m_inputs; }                 // 新增：可写输入端口
    const QList<Port>& inputs() const { return m_inputs; }
    QList<Port>& outputs() { return m_outputs; }
    const QList<Port>& outputs() const { return m_outputs; }

    QString nodeName() const { return m_name; }
    void setNodeName(const QString& name) { m_name = name; }
};