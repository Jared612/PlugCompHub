/**
 * @file pch.h
 * @brief PlugCompHub（PCH）核心公共头伞头：包含核心运行时全部对外接口
 * @details 可选插件（network / logger / sqlite / threadpool / application）的接口头按需单独包含，
 *          例如 `#include "network/ihttpclient.h"`。
 */
#pragma once

#include "core/core.h"
#include "core/interface.h"
#include "core/error.h"
#include "core/componentinfo.h"
#include "core/plugininfo.h"
#include "core/coreexport.h"
