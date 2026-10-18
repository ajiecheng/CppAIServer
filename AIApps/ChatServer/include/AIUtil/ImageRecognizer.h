// ImageRecognizer.h：本地图像分类：OpenCV 解码/缩放并构造浮点张量，ONNX Runtime 推理后返回最高分标签。
// 构造时读取模型输入尺寸和类别文件；同一实例可由用户映射复用，调用方需考虑并发访问。
// 预处理仅缩放到 0..1 后 blobFromImage，未显式 RGB 转换或均值方差归一化；是否符合模型需验证。

#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <onnxruntime_cxx_api.h>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <iostream>

class ImageRecognizer {
public:

    // 加载 ONNX 与标签，将输入/输出名称复制到 string，并读取 NCHW 输入尺寸；未校验模型输入维数或动态尺寸。
    explicit ImageRecognizer(const std::string& model_path,
        const std::string& label_path = "/root/imagenet_classes.txt");

    // 从磁盘解码图像，失败抛异常；成功后复用 PredictFromMat。
    std::string PredictFromFile(const std::string& image_path);

    // 对编码图像字节执行 imdecode，不是直接读取 Base64；失败抛异常。
    std::string PredictFromBuffer(const std::vector<unsigned char>& image_data);

    // 缩放、转浮点、blob 成 NCHW 后推理并取最大分数标签；未验证输出长度与标签数相等，也不返回概率。
    std::string PredictFromMat(const cv::Mat& img);

private:
    Ort::Env env;
    std::unique_ptr<Ort::Session> session;
    std::unique_ptr<Ort::AllocatorWithDefaultOptions> allocator;

    std::string input_name;
    std::string output_name;
    std::vector<int64_t> input_shape;
    int input_height{}, input_width{};

    std::vector<std::string> labels;

    // 按非空行加载类别名；文件无法打开或无有效标签时抛异常。
    void LoadLabels(const std::string& label_path);
};
