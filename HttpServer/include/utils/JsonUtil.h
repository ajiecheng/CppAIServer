// JsonUtil.h：统一引入 nlohmann/json.hpp，并用 json 别名供请求、响应和 AI 配置使用。
// 这里只提供类型，不执行 HTTP 解析或业务字段校验。

#pragma once

#include <nlohmann/json.hpp>

using json = nlohmann::json;
