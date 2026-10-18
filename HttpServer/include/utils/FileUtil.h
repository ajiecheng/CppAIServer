// FileUtil.h：静态页面文件读取辅助：以二进制方式打开文件、测长、读入调用方分配的 vector。
// 失败时可改读 NotFound.html；这只替换文件内容，不自动改变 HTTP 状态码。
// 路径相对进程工作目录；未验证回退文件打开结果，调用者须注意容量及读失败情况。

#pragma once

#include <fstream>
#include <string>
#include <vector>
#include <muduo/base/Logging.h>

class FileUtil
{
public:
    FileUtil(std::string filePath): filePath_(filePath), file_(filePath, std::ios::binary)
    {}

    ~FileUtil()
    {
        file_.close();
    }

    bool isValid() const
    { return file_.is_open(); }

    // 关闭原文件并打开回退 HTML；未再次校验打开结果，filePath_ 也未更新。
    void resetDefaultFile()
    {
        file_.close();
        file_.open("AIApps/ChatServer/resource/NotFound.html", std::ios::binary);
    }

    // seek 到末尾取长度再回到开头；tellg 失败未检查，不能把返回值无条件视为有效大小。
    uint64_t size()
    {
        file_.seekg(0, std::ios::end);
        uint64_t fileSize = file_.tellg();
        file_.seekg(0, std::ios::beg);
        return fileSize;
    }

    // 从文件读取到调用方 buffer；调用者须提前分配足够容量，失败这里只记录日志。
    void readFile(std::vector<char>& buffer)
    {
        if (file_.read(buffer.data(), size()))
        {
            LOG_INFO << "File content load into memory (" << size() << " bytes)";
        }
        else
        {
            LOG_ERROR << "File read failed";
        }
    }

private:
    std::string     filePath_;
    std::ifstream   file_;
};
