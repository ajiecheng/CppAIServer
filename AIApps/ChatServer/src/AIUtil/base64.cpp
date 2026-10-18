/*
   base64.cpp and base64.h

   base64 encoding and decoding with C++.
   More information at
     https://renenyffenegger.ch/notes/development/Base64/Encoding-and-decoding-base-64-with-cpp

   Version: 2.rc.09 (release candidate)

   Copyright (C) 2004-2017, 2020-2022 René Nyffenegger

   This source code is provided 'as-is', without any express or implied
   warranty. In no event will the author be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this source code must not be misrepresented; you must not
      claim that you wrote the original source code. If you use this source code
      in a product, an acknowledgment in the product documentation would be
      appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
      misrepresented as being the original source code.

   3. This notice may not be removed or altered from any source distribution.

   René Nyffenegger rene.nyffenegger@adp-gmbh.ch

*/
// 修改说明：仅调整解释性注释，编解码实现保持原样。
// base64.cpp：Base64 编解码辅助：上传图片时把文本还原为原始图像字节；编码不是加密。
// 支持标准与 URL 字符集，URL 模式补位使用点号；PEM/MIME 包装分别按 64/76 字符换行。
// string_view 重载避免输入转换为 string，但输出仍分配内存；非法或过短输入可能抛异常。
// 来源：René Nyffenegger；本次只重写解释性注释，原实现许可证完整保留。

#include "../include/AIUtil/base64.h"

#include <algorithm>
#include <stdexcept>

static const char* base64_chars[2] = {
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789"
             "+/",

             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789"
             "-_"};

// 把标准/URL 字符映射到 0..63，非法字符抛 runtime_error。
static unsigned int pos_of_char(const unsigned char chr) {

    if      (chr >= 'A' && chr <= 'Z') return chr - 'A';
    else if (chr >= 'a' && chr <= 'z') return chr - 'a' + ('Z' - 'A')               + 1;
    else if (chr >= '0' && chr <= '9') return chr - '0' + ('Z' - 'A') + ('z' - 'a') + 2;
    else if (chr == '+' || chr == '-') return 62;
    else if (chr == '/' || chr == '_') return 63;
    else

    throw std::runtime_error("Input is not valid base64-encoded data.");
}

// 在已编码字符串中按固定距离插入换行，返回修改后的副本。
static std::string insert_linebreaks(std::string str, size_t distance) {

    if (!str.length()) {
        return "";
    }

    size_t pos = distance;

    while (pos < str.size()) {
        str.insert(pos, "\n");
        pos += distance + 1;
    }

    return str;
}

template <typename String, unsigned int line_length>
// 先标准编码，再按模板行宽分行。
static std::string encode_with_line_breaks(String s) {
  return insert_linebreaks(base64_encode(s, false), line_length);
}

template <typename String>
// 包装 64 字符行宽编码，不生成 PEM 的 BEGIN/END 标记。
static std::string encode_pem(String s) {
  return encode_with_line_breaks<String, 64>(s);
}

template <typename String>
// 包装 76 字符行宽编码。
static std::string encode_mime(String s) {
  return encode_with_line_breaks<String, 76>(s);
}

template <typename String>
// 把 string/string_view 的原始字节转交公共编码函数。
static std::string encode(String s, bool url) {
  return base64_encode(reinterpret_cast<const unsigned char*>(s.data()), s.length(), url);
}

// 按三个输入字节产生四个字符，尾部补位；url=true 选 URL 字符集和点号补位。
std::string base64_encode(unsigned char const* bytes_to_encode, size_t in_len, bool url) {

    size_t len_encoded = (in_len +2) / 3 * 4;

    unsigned char trailing_char = url ? '.' : '=';

    const char* base64_chars_ = base64_chars[url];

    std::string ret;
    ret.reserve(len_encoded);

    unsigned int pos = 0;

    while (pos < in_len) {
        ret.push_back(base64_chars_[(bytes_to_encode[pos + 0] & 0xfc) >> 2]);

        if (pos+1 < in_len) {
           ret.push_back(base64_chars_[((bytes_to_encode[pos + 0] & 0x03) << 4) + ((bytes_to_encode[pos + 1] & 0xf0) >> 4)]);

           if (pos+2 < in_len) {
              ret.push_back(base64_chars_[((bytes_to_encode[pos + 1] & 0x0f) << 2) + ((bytes_to_encode[pos + 2] & 0xc0) >> 6)]);
              ret.push_back(base64_chars_[  bytes_to_encode[pos + 2] & 0x3f]);
           }
           else {
              ret.push_back(base64_chars_[(bytes_to_encode[pos + 1] & 0x0f) << 2]);
              ret.push_back(trailing_char);
           }
        }
        else {

            ret.push_back(base64_chars_[(bytes_to_encode[pos + 0] & 0x03) << 4]);
            ret.push_back(trailing_char);
            ret.push_back(trailing_char);
        }

        pos += 3;
    }

    return ret;
}

template <typename String>
// 每四个字符还原字节；可先移除 LF，接受等号/点号补位，长度与尾部严格校验并不完整。
static std::string decode(String const& encoded_string, bool remove_linebreaks) {

    if (encoded_string.empty()) return std::string();

    if (remove_linebreaks) {

       std::string copy(encoded_string);

       copy.erase(std::remove(copy.begin(), copy.end(), '\n'), copy.end());

       return base64_decode(copy, false);
    }

    size_t length_of_string = encoded_string.length();
    size_t pos = 0;

    size_t approx_length_of_decoded_string = length_of_string / 4 * 3;
    std::string ret;
    ret.reserve(approx_length_of_decoded_string);

    while (pos < length_of_string) {

       size_t pos_of_char_1 = pos_of_char(encoded_string.at(pos+1) );

       ret.push_back(static_cast<std::string::value_type>( ( (pos_of_char(encoded_string.at(pos+0)) ) << 2 ) + ( (pos_of_char_1 & 0x30 ) >> 4)));

       if ( ( pos + 2 < length_of_string  )       &&
              encoded_string.at(pos+2) != '='     &&
              encoded_string.at(pos+2) != '.'
          )
       {

          unsigned int pos_of_char_2 = pos_of_char(encoded_string.at(pos+2) );
          ret.push_back(static_cast<std::string::value_type>( (( pos_of_char_1 & 0x0f) << 4) + (( pos_of_char_2 & 0x3c) >> 2)));

          if ( ( pos + 3 < length_of_string )     &&
                 encoded_string.at(pos+3) != '='  &&
                 encoded_string.at(pos+3) != '.'
             )
          {

             ret.push_back(static_cast<std::string::value_type>( ( (pos_of_char_2 & 0x03 ) << 6 ) + pos_of_char(encoded_string.at(pos+3))   ));
          }
       }

       pos += 4;
    }

    return ret;
}

// 转交公共解码模板，返回可含 NUL 的二进制 string。
std::string base64_decode(std::string const& s, bool remove_linebreaks) {
   return decode(s, remove_linebreaks);
}

// 按三个输入字节产生四个字符，尾部补位；url=true 选 URL 字符集和点号补位。
std::string base64_encode(std::string const& s, bool url) {
   return encode(s, url);
}

// 提供对应输入类型的 64 字符换行包装。
std::string base64_encode_pem (std::string const& s) {
   return encode_pem(s);
}

// 提供对应输入类型的 76 字符换行包装。
std::string base64_encode_mime(std::string const& s) {
   return encode_mime(s);
}

#if __cplusplus >= 201703L

// 按三个输入字节产生四个字符，尾部补位；url=true 选 URL 字符集和点号补位。
std::string base64_encode(std::string_view s, bool url) {
   return encode(s, url);
}

// 提供对应输入类型的 64 字符换行包装。
std::string base64_encode_pem(std::string_view s) {
   return encode_pem(s);
}

// 提供对应输入类型的 76 字符换行包装。
std::string base64_encode_mime(std::string_view s) {
   return encode_mime(s);
}

// 转交公共解码模板，返回可含 NUL 的二进制 string。
std::string base64_decode(std::string_view s, bool remove_linebreaks) {
   return decode(s, remove_linebreaks);
}

#endif
