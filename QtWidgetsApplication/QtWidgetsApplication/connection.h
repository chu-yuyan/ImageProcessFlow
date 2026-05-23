#pragma once

class NodeItem;
class NodeConnection;

struct Connection
{
    NodeItem* fromNode = nullptr;
    int outputPortIndex = -1;
    NodeItem* toNode = nullptr;
    int inputPortIndex = -1;
    NodeConnection* lineItem = nullptr;

    Connection(NodeItem* from, int outIdx, NodeItem* to, int inIdx, NodeConnection* line)
        : fromNode(from), outputPortIndex(outIdx), toNode(to), inputPortIndex(inIdx), lineItem(line)
    {
    }
};