/********************************************************************************
 *
 * Copyright (c) 2018-2022 Advanced Micro Devices, Inc.
 *
 * MIT LICENSE:
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/


#ifndef DEB_PACKAGE_INFO_H
#define DEB_PACKAGE_INFO_H
#include "metaPackageInfo.h"

class DebPackageInfo : public PackageInfo{
  public:
    DebPackageInfo(const std::string& pkgname,
        const std::vector <std::string> cmd):PackageInfo(pkgname,
          std::string("dpkg"),
          cmd){}

    DebPackageInfo(const std::vector <std::string> cmd):PackageInfo(
          std::string("dpkg"),
          cmd){}

    bool readMetaPackageInfo(std::string ss) override;
    std::pair<std::string, std::string> getNameVers(std::string word);
    void readDepLine(const std::string& depLine);
};
#endif
