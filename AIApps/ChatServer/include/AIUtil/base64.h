// base64.h：Base64 编解码辅助：上传图片时把文本还原为原始图像字节；编码不是加密。
// 支持标准与 URL 字符集，URL 模式补位使用点号；PEM/MIME 包装分别按 64/76 字符换行。
// string_view 重载避免输入转换为 string，但输出仍分配内存；非法或过短输入可能抛异常。
// 来源：René Nyffenegger；本次只重写解释性注释，原实现许可证完整保留。

#ifndef BASE64_H_C0CE2A47_D10E_42C9_A27C_C883944E704A
#define BASE64_H_C0CE2A47_D10E_42C9_A27C_C883944E704A

#include <string>

#if __cplusplus >= 201703L
#include <string_view>
#endif

// 按三个输入字节产生四个字符，尾部补位；url=true 选 URL 字符集和点号补位。
std::string base64_encode(unsigned char const*, size_t len, bool url = false);
// 按三个输入字节产生四个字符，尾部补位；url=true 选 URL 字符集和点号补位。
std::string base64_encode(std::string const& s, bool url = false);
// 提供对应输入类型的 64 字符换行包装。
std::string base64_encode_pem(std::string const& s);
// 提供对应输入类型的 76 字符换行包装。
std::string base64_encode_mime(std::string const& s);

// 转交公共解码模板，返回可含 NUL 的二进制 string。
std::string base64_decode(std::string const& s, bool remove_linebreaks = false);

#if __cplusplus >= 201703L

// 按三个输入字节产生四个字符，尾部补位；url=true 选 URL 字符集和点号补位。
std::string base64_encode(std::string_view s, bool url = false);
// 提供对应输入类型的 64 字符换行包装。
std::string base64_encode_pem(std::string_view s);
// 提供对应输入类型的 76 字符换行包装。
std::string base64_encode_mime(std::string_view s);
// 转交公共解码模板，返回可含 NUL 的二进制 string。
std::string base64_decode(std::string_view s, bool remove_linebreaks = false);
#endif

#endif
