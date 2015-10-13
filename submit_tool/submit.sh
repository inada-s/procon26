#  The MIT License (MIT)
#  
#  Copyright (c) 2015 Shingo INADA
#  
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
#  
#  The above copyright notice and this permission notice shall be included in
#  all copies or substantial portions of the Software.
#  
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
#  THE SOFTWARE.

#!/bin/sh
source conf.sh
cd `dirname $0`
if [ $# -ne 1 ]; then
    echo "need problem id."
    exit 1
fi
FILE=$(find $1 -type f -name "*.ans*" -print | sort | head -n 1)
echo $FILE
if [ ${FILE: -2} == "ok" ]; then
    echo "$FILE is already submitted."
    exit 2
fi
URL="http://$SERVER_HOST/answer --form-string token=$TEAM_TOKEN -F answer=@$FILE"
echo $URL
curl $URL && mv "$FILE" "$FILE.ok"
