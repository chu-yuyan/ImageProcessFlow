#pragma once
#pragma once
#include <QString>
#include <QVariant>
#include <QMap>
#include <QList>
#include <opencv2/opencv.hpp>
#include <QUuid>          // 新增

enum class ImageType 
{ 
    Any,
    Color, 
    Gray, 
    PixelGrid,      // 像素化后的离散网格图像（每个块代表一个“像素块”）
    PaletteIndexed, // 索引色图像 + 调色板
    AlphaMasked,    // RGBA 图像，带透明通道
    BeadPattern     // 最终生成的拼豆图纸（可视化网格+色号） 
};

struct Port
{
    QString name;
    ImageType type;
    QVariant data;


};

// 参数元数据
struct ParameterMeta
{
    int typeId = QMetaType::UnknownType;
    QVariant minimum;
    QVariant maximum;
    QVariant singleStep;
    QVariant defaultValue;
};

class BaseNode
{
protected:
    QUuid m_id{ QUuid::createUuid() };
    QString m_name;

    QList<Port> m_inputs;
    QList<Port> m_outputs;

public:
    virtual ~BaseNode() = default;
    virtual void process() = 0;
    virtual QString typeName() const = 0;

    QUuid id() const { return m_id; }

    virtual QMap<QString, QVariant> getParameters() const { return {}; }
    virtual void setParameter(const QString& name, const QVariant& value) {}

    virtual QMap<QString, ParameterMeta> getParameterMeta() const { return {}; }

    QList<Port>& inputs() { return m_inputs; }
    const QList<Port>& inputs() const { return m_inputs; }
    QList<Port>& outputs() { return m_outputs; }
    const QList<Port>& outputs() const { return m_outputs; }

    QString nodeName() const { return m_name; }
    void setNodeName(const QString& name) { m_name = name; }
};