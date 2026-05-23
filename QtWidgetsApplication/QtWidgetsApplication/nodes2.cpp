#include"nodes2.h"
#include <QHttpMultiPart>
#include<qtimer.h>


static void drawGridWithNumbers(cv::Mat& canvas, const cv::Mat& src,
    int cellSize, int fontSize, bool showColor,
    std::function<std::string(int row, int col, uchar idx, const cv::Vec3b& color)> getNumber) {
    int rows = src.rows;
    int cols = src.cols;
    int cellW = cellSize;
    int cellH = cellSize;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            int x = c * cellW;
            int y = r * cellH;
            cv::Rect cellRect(x, y, cellW, cellH);

            cv::Vec3b color = cv::Vec3b(255, 255, 255);
            uchar idx = 0;
            if (src.type() == CV_8UC3) {
                color = src.at<cv::Vec3b>(r, c);
            }
            else if (src.type() == CV_8UC1) {
                idx = src.at<uchar>(r, c);
                // 暂时不获取颜色，由函数内部根据 idx 决定（如果有 palette 则另行传递）
            }

            if (showColor && src.type() == CV_8UC3) {
                cv::rectangle(canvas, cellRect, cv::Scalar(color[0], color[1], color[2]), cv::FILLED);
            }
            else {
                // 仅画边框，内部白色
                cv::rectangle(canvas, cellRect, cv::Scalar(255, 255, 255), cv::FILLED);
                cv::rectangle(canvas, cellRect, cv::Scalar(0, 0, 0), 1);
            }

            std::string number = getNumber(r, c, idx, color);
            if (!number.empty()) {
                int baseline = 0;
                double fontScale = fontSize / 20.0;
                cv::Size textSize = cv::getTextSize(number, cv::FONT_HERSHEY_SIMPLEX,
                    fontScale, 1, &baseline);
                cv::Point textOrg(x + (cellW - textSize.width) / 2,
                    y + (cellH + textSize.height) / 2);
                cv::putText(canvas, number, textOrg, cv::FONT_HERSHEY_SIMPLEX,
                    fontScale, cv::Scalar(0, 0, 0), 1, cv::LINE_AA);
            }
        }
    }
}

cv::Mat BeadPatternNode::drawGridFromIndexed(const ImagePacket& packet, int cellSize, int fontSize, bool showColor) {
    cv::Mat indexed = packet.indexMap;
    if (indexed.empty() && packet.type == ImageType::PaletteIndexed) {
        // 降级：从彩色图重新匹配
        indexed = cv::Mat(packet.image.size(), CV_8UC1);
        const auto& pal = packet.palette;
        for (int y = 0; y < packet.image.rows; ++y) {
            for (int x = 0; x < packet.image.cols; ++x) {
                cv::Vec3b c = packet.image.at<cv::Vec3b>(y, x);
                int best = 0, bestDist = INT_MAX;
                for (int i = 0; i < pal.size(); ++i) {
                    int dr = c[0] - pal[i][0];
                    int dg = c[1] - pal[i][1];
                    int db = c[2] - pal[i][2];
                    int d = dr * dr + dg * dg + db * db;
                    if (d < bestDist) { bestDist = d; best = i; }
                }
                indexed.at<uchar>(y, x) = best;
            }
        }
    }
    if (indexed.empty()) return packet.image;

    cv::Mat canvas = cv::Mat(indexed.rows * cellSize, indexed.cols * cellSize, CV_8UC3, cv::Scalar(255, 255, 255));

    // 构造颜色映射（用于显示填充）
    cv::Mat colorMat(indexed.size(), CV_8UC3);
    for (int y = 0; y < indexed.rows; ++y) {
        for (int x = 0; x < indexed.cols; ++x) {
            int idx = indexed.at<uchar>(y, x);
            if (idx >= 0 && idx < packet.palette.size())
                colorMat.at<cv::Vec3b>(y, x) = packet.palette[idx];
            else
                colorMat.at<cv::Vec3b>(y, x) = cv::Vec3b(0, 0, 0);
        }
    }

    // 使用传入的 colorCodes 显示色号（如 "A1", "B12"）
    auto getNumber = [&](int row, int col, uchar idx, const cv::Vec3b& color) -> std::string {
        (void)row; (void)col; (void)color;
        if (idx >= 0 && idx < packet.colorCodes.size())
            return packet.colorCodes[idx].toStdString();
        else
            return "?";
        };

    drawGridWithNumbers(canvas, colorMat, cellSize, fontSize, showColor, getNumber);
    return canvas;
}


cv::Mat BeadPatternNode::drawGridFromRGB(const cv::Mat& rgb, int cellSize, int fontSize, bool showColor)
{
    if (rgb.empty())
        return rgb;

    // 期望输入是 CV_8UC3（BGR），如果不是则尽量转换
    cv::Mat src;
    if (rgb.type() == CV_8UC3) {
        src = rgb;
    }
    else if (rgb.type() == CV_8UC4) {
        cv::cvtColor(rgb, src, cv::COLOR_BGRA2BGR);
    }
    else if (rgb.type() == CV_8UC1) {
        cv::cvtColor(rgb, src, cv::COLOR_GRAY2BGR);
    }
    else {
        // 其他类型不支持，直接返回原图，避免崩溃
        return rgb;
    }

    // 这里的 src 被当做“网格数据”：每个像素对应一个格子
    cv::Mat canvas(src.rows * cellSize, src.cols * cellSize, CV_8UC3, cv::Scalar(255, 255, 255));

    // 如果没有调色板索引，编号策略：显示行列（也可以改成显示空字符串）
    auto getNumber = [&](int row, int col, uchar idx, const cv::Vec3b& color) -> std::string {
        (void)idx;
        (void)color;

        // 简单编号：1-based 行列，如 "3,5"
        // 如果你不想显示编号，return "" 即可
        char buf[32];
        snprintf(buf, sizeof(buf), "%d,%d", row + 1, col + 1);
        return std::string(buf);
    };

    drawGridWithNumbers(canvas, src, cellSize, fontSize, showColor, getNumber);
    return canvas;
}

void RemoveBackgroundNode::process()
{
    auto packet = m_inputs[0].data.value<ImagePacket>();
    if (packet.image.empty()) {
        qDebug() << "RemoveBackgroundNode: empty input";
        return;
    }
    if (m_apiKey.isEmpty()) {
        qDebug() << "RemoveBackgroundNode: no API key, returning original image";
        m_outputs[0].data = QVariant::fromValue(packet);
        return;
    }

    // 1. 将 cv::Mat 编码为 PNG 字节数组
    std::vector<uchar> buf;
    cv::imencode(".png", packet.image, buf);
    QByteArray imageData(reinterpret_cast<const char*>(buf.data()), buf.size());

    // 2. 构建 multipart/form-data 请求
    QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart imagePart;
    imagePart.setHeader(QNetworkRequest::ContentTypeHeader, "image/png");
    imagePart.setHeader(QNetworkRequest::ContentDispositionHeader,
        QVariant("form-data; name=\"image_file\"; filename=\"image.png\""));
    imagePart.setBody(imageData);
    multiPart->append(imagePart);

    // 可选：设置输出格式为 PNG，保留透明背景
    QHttpPart formatPart;
    formatPart.setHeader(QNetworkRequest::ContentDispositionHeader,
        QVariant("form-data; name=\"format\""));
    formatPart.setBody("png");
    multiPart->append(formatPart);

    // 3. 发送请求
    QUrl url("https://api.remove.bg/v1.0/removebg");
    QNetworkRequest request(url);
    request.setRawHeader("Authorization",
        QString("Bearer %1").arg(m_apiKey).toUtf8());

    QNetworkReply* reply = m_nam.post(request, multiPart);
    multiPart->setParent(reply); // 请求结束后自动释放

    // 4. 同步等待（仅用于命令行模式，GUI 模式会阻塞）
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    timer.start(10000);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    // 5. 处理响应
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray pngData = reply->readAll();
        // 将 PNG 数据解码为 cv::Mat (RGBA)
        cv::Mat rgba = cv::imdecode(std::vector<uchar>(pngData.begin(), pngData.end()),
            cv::IMREAD_UNCHANGED);
        if (!rgba.empty()) {
            ImagePacket out;
            out.image = rgba;
            out.type = ImageType::AlphaMasked;
            out.hasAlpha = (rgba.channels() == 4);
            m_outputs[0].data = QVariant::fromValue(out);
            qDebug() << "RemoveBackgroundNode: background removed successfully";
        }
        else {
            qDebug() << "RemoveBackgroundNode: failed to decode response image";
            m_outputs[0].data = QVariant::fromValue(packet);
        }
    }
    else {
        qDebug() << "RemoveBackgroundNode API error:" << reply->errorString();
        m_outputs[0].data = QVariant::fromValue(packet);
    }
    reply->deleteLater();
}
