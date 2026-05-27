#include"nodes2.h"
#include <QHttpMultiPart>
#include<qtimer.h>

namespace 
{
static void loadGameBoyPalette(QVector<cv::Vec3b>& palette, QVector<QString>& colorCodes)
{
    palette = { {0x0F,0x38,0x0F}, {0x30,0x62,0x30}, {0x8B,0xAC,0x0F}, {0x9B,0xBC,0x0F} };
    colorCodes = { "Dark", "Mid", "Light", "Lighter" };
}
} // namespace

void DitherNode::process()
{
    auto packet = m_inputs[0].data.value<ImagePacket>();
    if (packet.type != ImageType::PaletteIndexed || packet.image.empty()) return;

    cv::Mat originalColor = packet.image;  // 重建的彩色图
    cv::Mat indexMap = packet.indexMap;    // 索引图
    const auto& palette = packet.palette;  // 调色板

    if (originalColor.empty() || indexMap.empty() || palette.isEmpty()) return;

    // 转換為浮點型以存儲誤差
    cv::Mat floatImage;
    originalColor.convertTo(floatImage, CV_32FC3);

    // 误差扩散矩阵
    const int fs_matrix[4][2] = { {0, 1}, {1, -1}, {1, 0}, {1, 1} };
    const float fs_weights[4] = { 7.0f / 16, 3.0f / 16, 5.0f / 16, 1.0f / 16 };

    cv::Mat resultIndex = cv::Mat::zeros(indexMap.size(), CV_8UC1);

    for (int y = 0; y < floatImage.rows; ++y) {
        for (int x = 0; x < floatImage.cols; ++x) {
            cv::Vec3f original = floatImage.at<cv::Vec3f>(y, x);

            // 找调色板中最接近的颜色
            int bestIdx = 0;
            float bestDist = FLT_MAX;
            for (int i = 0; i < palette.size(); ++i) {
                cv::Vec3f palColor(palette[i][0], palette[i][1], palette[i][2]);
                float dr = original[0] - palColor[0];
                float dg = original[1] - palColor[1];
                float db = original[2] - palColor[2];
                float dist = dr * dr + dg * dg + db * db;
                if (dist < bestDist) {
                    bestDist = dist;
                    bestIdx = i;
                }
            }

            resultIndex.at<uchar>(y, x) = bestIdx;
            cv::Vec3f quantized(palette[bestIdx][0], palette[bestIdx][1], palette[bestIdx][2]);
            cv::Vec3f error = original - quantized;

            // 扩散误差
            for (int i = 0; i < 4; ++i) {
                int nx = x + fs_matrix[i][0];
                int ny = y + fs_matrix[i][1];
                if (nx >= 0 && nx < floatImage.cols && ny >= 0 && ny < floatImage.rows) {
                    floatImage.at<cv::Vec3f>(ny, nx) += error * fs_weights[i];
                }
            }
        }
    }

    // 构造输出
    cv::Mat colorResult(resultIndex.size(), CV_8UC3);
    for (int y = 0; y < resultIndex.rows; ++y) {
        for (int x = 0; x < resultIndex.cols; ++x) {
            int idx = resultIndex.at<uchar>(y, x);
            colorResult.at<cv::Vec3b>(y, x) = palette[idx];
        }
    }

    ImagePacket out;
    out.image = colorResult;
    out.indexMap = resultIndex;
    out.type = ImageType::PaletteIndexed;
    out.paletteName = packet.paletteName;
    out.palette = packet.palette;
    out.colorCodes = packet.colorCodes;

    m_outputs[0].data = QVariant::fromValue(out);
}

void PaletteReduceNode::loadPalette(const QString& name)
{
    m_palette.clear();
    m_colorCodes.clear();

    // -------- 经典调色板 ----------
    if (name == "GameBoy") {
        loadGameBoyPalette(m_palette, m_colorCodes);
    }
    else if (name == "BlackWhite") {
        m_palette = { {0,0,0}, {255,255,255} };
        m_colorCodes = { "Black", "White" };
    }
    else if (name == "CGA") {
        m_palette = { {0,0,0}, {0,255,255}, {255,0,255}, {255,255,255} };
        m_colorCodes = { "Black", "Cyan", "Magenta", "White" };
    }
    else if (name == "EGA") {
        m_palette = {
            {0,0,0}, {0,0,170}, {0,170,0}, {0,170,170},
            {170,0,0}, {170,0,170}, {170,170,0}, {170,170,170},
            {85,85,85}, {85,85,255}, {85,255,85}, {85,255,255},
            {255,85,85}, {255,85,255}, {255,255,85}, {255,255,255}
        };
        for (int i = 0; i < m_palette.size(); ++i)
            m_colorCodes.push_back(QString("Color%1").arg(i));
    }
    else if (name == "PICO8") {
        m_palette = {
            {0,0,0}, {29,43,83}, {126,37,83}, {0,135,81},
            {171,82,54}, {95,87,79}, {194,195,199}, {255,241,232},
            {255,0,77}, {255,163,0}, {255,236,39}, {0,228,54},
            {41,173,255}, {131,118,156}, {255,119,168}, {255,204,170}
        };
        m_colorCodes = { "Black","DarkBlue","DarkPurple","DarkGreen",
                         "Brown","DarkGray","LightGray","White",
                         "Red","Orange","Yellow","Green","Blue","Lavender","Pink","Peach" };
    }
    else if (name == "Commodore64") {
        m_palette = {
            {0,0,0}, {255,255,255}, {136,0,0}, {170,255,238},
            {204,68,204}, {0,204,85}, {0,0,170}, {238,238,119},
            {221,136,85}, {102,68,0}, {255,119,119}, {51,51,51},
            {119,119,119}, {170,255,102}, {0,136,255}, {187,187,187}
        };
        m_colorCodes = { "Black","White","Red","Cyan","Purple","Green","Blue","Yellow",
                         "Orange","Brown","Pink","DarkGray","MediumGray","LightGreen","LightBlue","LightGray" };
    }
    else if (name == "Arne16") {
        m_palette = {
            {0,0,0}, {157,157,157}, {255,255,255}, {190,38,51},
            {224,111,139}, {73,60,43}, {164,100,34}, {235,137,49},
            {247,226,107}, {47,72,78}, {68,137,26}, {163,206,39},
            {27,133,154}, {87,124,194}, {182,107,202}, {212,174,141}
        };
        m_colorCodes = { "Black","Gray","White","Red","Pink","Brown","Orange","Yellow",
                         "LightYellow","DarkTeal","Green","LightGreen","Teal","Blue","Purple","Skin" };
    }
    else if (name == "Endesga32") {
        m_palette = {
            {0,0,0}, {34,32,52}, {69,40,60}, {102,57,49},
            {143,86,59}, {223,113,38}, {217,160,102}, {238,195,154},
            {104,92,86}, {73,131,96}, {110,182,96}, {169,219,108},
            {55,118,130}, {69,162,173}, {132,206,212}, {207,235,226}
        };
        for (int i = 0; i < m_palette.size(); ++i)
            m_colorCodes.push_back(QString("E32_%1").arg(i));
    }
    else if (name == "AAP-64") {
        m_palette = {
            {84,84,84}, {136,136,136}, {188,188,188}, {240,240,240},
            {100,60,40}, {156,100,60}, {212,148,88}, {252,196,120},
            {76,48,96}, {124,80,128}, {172,120,160}, {220,168,196},
            {64,92,112}, {108,132,148}, {152,176,184}, {200,220,212}
        };
        for (int i = 0; i < m_palette.size(); ++i)
            m_colorCodes.push_back(QString("AAP%1").arg(i));
    }
    else if (name == "Mard221") {
        QFile file("mard221.json");
        if (!file.open(QIODevice::ReadOnly)) {
            qDebug() << "Failed to open mard221.json, using GameBoy fallback";
            loadGameBoyPalette(m_palette, m_colorCodes);
            return;
        }

        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject root = doc.object();
        QJsonArray colors = root["colors"].toArray();

        for (const QJsonValue& val : colors) {
            QJsonObject obj = val.toObject();
            QString code = obj["code"].toString();
            QJsonArray rgbArr = obj["rgb"].toArray();

            // 3) 修正：JSON 是 RGB，但 OpenCV Vec3b 是 BGR
            const int r = rgbArr.size() > 0 ? rgbArr[0].toInt() : 0;
            const int g = rgbArr.size() > 1 ? rgbArr[1].toInt() : 0;
            const int b = rgbArr.size() > 2 ? rgbArr[2].toInt() : 0;
            cv::Vec3b bgr(static_cast<uchar>(b), static_cast<uchar>(g), static_cast<uchar>(r));

            m_palette.push_back(bgr);
            m_colorCodes.push_back(code);
        }

        qDebug() << "Loaded Mard221 palette with" << m_palette.size() << "colors";
    }
    else {
        qDebug() << "Unknown palette name '" << name << "', using GameBoy";
        loadGameBoyPalette(m_palette, m_colorCodes);
    }
}
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

void RemoveBackgroundNode::process()
{
    auto packet = m_inputs[0].data.value<ImagePacket>();
    if (packet.image.empty()) {
        qDebug() << "RemoveBackgroundNode: empty input";
        return;
    }

    cv::Mat inputImage;

    if (packet.type == ImageType::PaletteIndexed) {
        if (!packet.indexMap.empty() && !packet.palette.empty()) {
            inputImage = cv::Mat(packet.indexMap.size(), CV_8UC3);
            for (int y = 0; y < packet.indexMap.rows; ++y) {
                for (int x = 0; x < packet.indexMap.cols; ++x) {
                    int idx = packet.indexMap.at<uchar>(y, x);
                    inputImage.at<cv::Vec3b>(y, x) = packet.palette[idx];
                }
            }
        } else {
            inputImage = packet.image;
        }
    } else if (packet.type == ImageType::PixelGrid) {
        inputImage = packet.image;
    } else {
        inputImage = packet.image;
    }

    if (inputImage.channels() == 4) {
        cv::cvtColor(inputImage, inputImage, cv::COLOR_BGRA2BGR);
    }

    if (m_apiKey.isEmpty()) {
        qDebug() << "RemoveBackgroundNode: no API key, returning original image";
        m_outputs[0].data = QVariant::fromValue(packet);
        return;
    }

    std::vector<uchar> buf;
    cv::imencode(".png", inputImage, buf);
    QByteArray imageData(reinterpret_cast<const char*>(buf.data()), (int)buf.size());

    QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart imagePart;
    imagePart.setHeader(QNetworkRequest::ContentTypeHeader, "image/png");
    imagePart.setHeader(QNetworkRequest::ContentDispositionHeader,
        QVariant("form-data; name=\"image_file\"; filename=\"image.png\""));
    imagePart.setBody(imageData);
    multiPart->append(imagePart);

    QHttpPart formatPart;
    formatPart.setHeader(QNetworkRequest::ContentDispositionHeader,
        QVariant("form-data; name=\"format\""));
    formatPart.setBody("png");
    multiPart->append(formatPart);

    QUrl url("https://api.remove.bg/v1.0/removebg");
    QNetworkRequest request(url);
    request.setRawHeader("X-Api-Key", m_apiKey.toUtf8());

    QNetworkReply* reply = m_nam.post(request, multiPart);
    multiPart->setParent(reply);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    timer.start(30000);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray pngData = reply->readAll();
        cv::Mat decoded = cv::imdecode(std::vector<uchar>(pngData.begin(), pngData.end()),
            cv::IMREAD_UNCHANGED);

        if (!decoded.empty()) {
            ImagePacket out;

            // 关键：如果返回带 alpha，把 alphaMask 填上，并保证 image 有可显示的 BGR
            if (decoded.channels() == 4) {
                cv::Mat bgr;
                cv::cvtColor(decoded, bgr, cv::COLOR_BGRA2BGR);

                std::vector<cv::Mat> ch;
                cv::split(decoded, ch);          // BGRA -> ch[3] 是 alpha
                out.alphaMask = ch[3].clone();   // CV_8UC1

                out.image = bgr;
                out.hasAlpha = true;
                out.type = ImageType::AlphaMasked;
            } else {
                out.image = decoded;
                out.hasAlpha = false;
                out.type = ImageType::Color;
                out.alphaMask.release();
            }

            m_outputs[0].data = QVariant::fromValue(out);
            qDebug() << "RemoveBackgroundNode: background removed successfully"
                     << "channels=" << decoded.channels()
                     << "size=" << decoded.cols << "x" << decoded.rows;
        } else {
            qDebug() << "RemoveBackgroundNode: failed to decode response image";
            m_outputs[0].data = QVariant::fromValue(packet);
        }
    } else {
        qDebug() << "RemoveBackgroundNode API error:" << reply->errorString();
        m_outputs[0].data = QVariant::fromValue(packet);
    }
    reply->deleteLater();
}

void BeadPatternNode::process() {
    auto packet = m_inputs[0].data.value<ImagePacket>();
    if (packet.image.empty()) return;

    if (packet.type != ImageType::PixelGrid) {
        qDebug() << "BeadPatternNode: only accepts PixelGrid input";
        return;
    }

    cv::Mat src = packet.image;
    int rows = src.rows;
    int cols = src.cols;
    int cellW = m_cellSize;
    int cellH = m_cellSize;

    // 自动计算字体大小：格子大小的 35-40%
    int actualFontSize = std::max(8, std::min(m_fontSize, (int)(m_cellSize * 0.4)));
    double fontScale = actualFontSize / 20.0;

    // 设置文字粗细
    int fontThickness = std::max(1, actualFontSize / 10);

    cv::Mat canvas(rows * cellW, cols * cellH, CV_8UC3, cv::Scalar(255, 255, 255));

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            int x = c * cellW;
            int y = r * cellH;
            cv::Rect cellRect(x, y, cellW, cellH);

            cv::Vec3b color = src.at<cv::Vec3b>(r, c);

            // 画颜色块
            if (m_showColor) {
                cv::rectangle(canvas, cellRect, cv::Scalar(color[0], color[1], color[2]), cv::FILLED);
            }
            else {
                cv::rectangle(canvas, cellRect, cv::Scalar(255, 255, 255), cv::FILLED);
                cv::rectangle(canvas, cellRect, cv::Scalar(0, 0, 0), 1);
            }

            // 找最接近的调色板颜色，获取色号
            int bestIdx = 0;
            int bestDist = INT_MAX;
            for (int i = 0; i < m_palette.size(); ++i) {
                int dr = color[0] - m_palette[i][0];
                int dg = color[1] - m_palette[i][1];
                int db = color[2] - m_palette[i][2];
                int dist = dr * dr + dg * dg + db * db;
                if (dist < bestDist) {
                    bestDist = dist;
                    bestIdx = i;
                }
            }

            QString code = (bestIdx >= 0 && bestIdx < m_colorCodes.size()) ? m_colorCodes[bestIdx] : "?";

            // 如果文字太长，只显示前3个字符 + "."
            std::string text = code.toStdString();
            int maxChars = cellW / (actualFontSize * 0.6);
            if (text.length() > maxChars && maxChars >= 3) {
                text = text.substr(0, maxChars - 1) + ".";
            }

            // 计算文字位置（居中）
            int baseline = 0;
            cv::Size textSize = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX,
                fontScale, fontThickness, &baseline);

            // 如果文字还是太大，再次缩小
            if (textSize.width > cellW - 4) {
                fontScale = (cellW - 4) * 1.0 / textSize.width * fontScale;
                textSize = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX,
                    fontScale, fontThickness, &baseline);
            }

            cv::Point textOrg(x + (cellW - textSize.width) / 2,
                y + (cellH + textSize.height) / 2);

            // 根据背景亮度决定文字颜色（更精确的亮度计算）
            int brightness = 0.299 * color[2] + 0.587 * color[1] + 0.114 * color[0];
            cv::Scalar textColor = (brightness < 128) ? cv::Scalar(255, 255, 255) : cv::Scalar(0, 0, 0);

            // 使用 LINE_AA 抗锯齿
            cv::putText(canvas, text, textOrg, cv::FONT_HERSHEY_SIMPLEX,
                fontScale, textColor, fontThickness, cv::LINE_AA);
        }
    }

    ImagePacket out(canvas, ImageType::BeadPattern);
    m_outputs[0].data = QVariant::fromValue(out);
}