#ifndef TEXT_ENCODING_DETECT_H_
#define TEXT_ENCODING_DETECT_H_

//
// Copyright 2015 Jonathan Bennett <jon@autoitscript.com>
// 
// https://www.autoitscript.com 
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or 
// implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Modifed by Malcolm McLean
//
 
typedef enum
{
    TEXTENC_None,                // Unknown or binary
    TEXTENC_ANSI,                // 0-255
    TEXTENC_ASCII,                // 0-127
    TEXTENC_UTF8_BOM,            // UTF8 with BOM
    TEXTENC_UTF8_NOBOM,            // UTF8 without BOM
    TEXTENC_UTF16_LE_BOM,        // UTF16 LE with BOM
    TEXTENC_UTF16_LE_NOBOM,        // UTF16 LE without BOM
    TEXTENC_UTF16_BE_BOM,        // UTF16-BE with BOM
    TEXTENC_UTF16_BE_NOBOM,        // UTF16-BE without BOM
} TextEncoding;

TextEncoding DetectTextFileEncoding(const char *filename, int *error);

#endif

