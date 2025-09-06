![platform](https://img.shields.io/static/v1?label=platform&message=mac-intel%20|%20mac-arm%20|%20win-64&color=blue)
[![license](https://img.shields.io/github/license/miyako/pff-parser)](LICENSE)
![downloads](https://img.shields.io/github/downloads/miyako/pff-parser/total)

### Dependencies and Licensing

* the source code of this CLI tool is licensed under the MIT license.
* see [libpff](https://github.com/libyal/libpff/blob/main/COPYING) for the licensing of **libpff** (LGPL-3.0).

# pff-parser
CLI tool to extract text from PST

## usage

```
pff-parser -i example.pst -o example.json

-i path  : document to parse
-o path  : text output (default=stdout)
-        : use stdin for input
-r       : raw text output (default=json)
```

## output (JSON)

```
{
    "type: "ost" | "pst" | "pab",
    "folder": [
        {
            "folders": [
                {
                    name: "name",
                    messages: [
                        {
                            sender: {name: "name", address: "address"},
                            subject: "subject",
                            text: "text",
                        }
                        ],
                    folders: []
                }
            ]
        }
    ]
}
```
