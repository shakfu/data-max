{
    "patcher": {
        "fileversion": 1,
        "appversion": {
            "major": 9,
            "minor": 1,
            "revision": 2,
            "architecture": "x64",
            "modernui": 1
        },
        "classnamespace": "box",
        "rect": [ 100.0, 100.0, 853.0, 613.0 ],
        "boxes": [
            {
                "box": {
                    "fontface": 1,
                    "fontsize": 16.0,
                    "id": "obj-comment-title",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 15.0, 10.0, 565.0, 24.0 ],
                    "text": "dataframe - pandas-like columnar data for Max"
                }
            },
            {
                "box": {
                    "id": "obj-comment-io",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 110.0, 77.0, 200.0, 20.0 ],
                    "text": "--- I/O ---"
                }
            },
            {
                "box": {
                    "id": "obj-read",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 125.0, 102.0, 96.0, 22.0 ],
                    "text": "read sample.csv"
                }
            },
            {
                "box": {
                    "id": "obj-write",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 395.0, 102.0, 220.0, 22.0 ],
                    "text": "write /tmp/out.csv"
                }
            },
            {
                "box": {
                    "id": "obj-clear",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 635.0, 102.0, 40.0, 22.0 ],
                    "text": "clear"
                }
            },
            {
                "box": {
                    "id": "obj-comment-inspect",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 110.0, 137.0, 200.0, 20.0 ],
                    "text": "--- Inspection ---"
                }
            },
            {
                "box": {
                    "id": "obj-bang",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 125.0, 162.0, 24.0, 24.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-shape",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 165.0, 162.0, 45.0, 22.0 ],
                    "text": "shape"
                }
            },
            {
                "box": {
                    "id": "obj-columns",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 225.0, 162.0, 55.0, 22.0 ],
                    "text": "columns"
                }
            },
            {
                "box": {
                    "id": "obj-head",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 295.0, 162.0, 50.0, 22.0 ],
                    "text": "head 5"
                }
            },
            {
                "box": {
                    "id": "obj-tail",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 360.0, 162.0, 45.0, 22.0 ],
                    "text": "tail 3"
                }
            },
            {
                "box": {
                    "id": "obj-getcol",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 420.0, 162.0, 80.0, 22.0 ],
                    "text": "getcol score"
                }
            },
            {
                "box": {
                    "id": "obj-comment-stats",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 110.0, 202.0, 200.0, 20.0 ],
                    "text": "--- Statistics ---"
                }
            },
            {
                "box": {
                    "id": "obj-mean",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 125.0, 227.0, 75.0, 22.0 ],
                    "text": "mean score"
                }
            },
            {
                "box": {
                    "id": "obj-median",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 215.0, 227.0, 85.0, 22.0 ],
                    "text": "median score"
                }
            },
            {
                "box": {
                    "id": "obj-std",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 315.0, 227.0, 65.0, 22.0 ],
                    "text": "std score"
                }
            },
            {
                "box": {
                    "id": "obj-sum",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 395.0, 227.0, 70.0, 22.0 ],
                    "text": "sum score"
                }
            },
            {
                "box": {
                    "id": "obj-min",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 480.0, 227.0, 65.0, 22.0 ],
                    "text": "min score"
                }
            },
            {
                "box": {
                    "id": "obj-max",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 560.0, 227.0, 65.0, 22.0 ],
                    "text": "max score"
                }
            },
            {
                "box": {
                    "id": "obj-count",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 640.0, 227.0, 80.0, 22.0 ],
                    "text": "count score"
                }
            },
            {
                "box": {
                    "id": "obj-describe",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 735.0, 227.0, 95.0, 22.0 ],
                    "text": "describe score"
                }
            },
            {
                "box": {
                    "id": "obj-comment-filter",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 110.0, 267.0, 200.0, 20.0 ],
                    "text": "--- Filtering ---"
                }
            },
            {
                "box": {
                    "id": "obj-sel",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 125.0, 292.0, 120.0, 22.0 ],
                    "text": "sel score > 85"
                }
            },
            {
                "box": {
                    "id": "obj-df",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "" ],
                    "patching_rect": [ 295.0, 367.0, 273.0, 22.0 ],
                    "text": "dataframe mydata"
                }
            },
            {
                "box": {
                    "id": "obj-data-out",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 133.0, 471.0, 181.0, 22.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-info-out",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 385.0, 471.0, 183.0, 22.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-loadbang",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "patching_rect": [ 133.0, 411.0, 58.0, 22.0 ],
                    "text": "loadbang"
                }
            },
            {
                "box": {
                    "id": "obj-set-data",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 133.0, 443.0, 29.5, 22.0 ],
                    "text": "set"
                }
            },
            {
                "box": {
                    "id": "obj-set-info",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 385.0, 441.0, 29.5, 22.0 ],
                    "text": "set"
                }
            },
            {
                "box": {
                    "id": "obj-loadbang2",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "patching_rect": [ 385.0, 411.0, 58.0, 22.0 ],
                    "text": "loadbang"
                }
            },
            {
                "box": {
                    "id": "obj-comment-data",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 133.0, 503.0, 150.0, 20.0 ],
                    "text": "data outlet (left)"
                }
            },
            {
                "box": {
                    "id": "obj-comment-info",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 385.0, 501.0, 150.0, 20.0 ],
                    "text": "info outlet (right)"
                }
            }
        ],
        "lines": [
            {
                "patchline": {
                    "destination": [ "obj-df", 0 ],
                    "source": [ "obj-bang", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-df", 0 ],
                    "source": [ "obj-clear", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-df", 0 ],
                    "source": [ "obj-columns", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-df", 0 ],
                    "source": [ "obj-count", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-df", 0 ],
                    "source": [ "obj-describe", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-data-out", 1 ],
                    "source": [ "obj-df", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-info-out", 1 ],
                    "source": [ "obj-df", 1 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-df", 0 ],
                    "source": [ "obj-getcol", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-df", 0 ],
                    "source": [ "obj-head", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-set-data", 0 ],
                    "source": [ "obj-loadbang", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-set-info", 0 ],
                    "source": [ "obj-loadbang2", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-df", 0 ],
                    "source": [ "obj-max", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-df", 0 ],
                    "source": [ "obj-mean", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-df", 0 ],
                    "source": [ "obj-median", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-df", 0 ],
                    "source": [ "obj-min", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-df", 0 ],
                    "source": [ "obj-read", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-df", 0 ],
                    "source": [ "obj-sel", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-data-out", 0 ],
                    "source": [ "obj-set-data", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-info-out", 0 ],
                    "source": [ "obj-set-info", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-df", 0 ],
                    "source": [ "obj-shape", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-df", 0 ],
                    "source": [ "obj-std", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-df", 0 ],
                    "source": [ "obj-sum", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-df", 0 ],
                    "source": [ "obj-tail", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-df", 0 ],
                    "source": [ "obj-write", 0 ]
                }
            }
        ],
        "autosave": 0
    }
}