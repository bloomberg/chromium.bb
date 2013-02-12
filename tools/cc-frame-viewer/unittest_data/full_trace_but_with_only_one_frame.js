/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
window.gFullTraceButWithOnlyOneFrame = {
  "clientInfo": {
    "version": "Chrome/26.0.1400.0",
  },
  "systemTraceEvents": [],
  "traceEvents": [
    {
      "name": "ChannelReader::DispatchInputData",
      "args": {
        "line": 28,
        "class": 62
      },
      "pid": 4717,
      "ts": 1619171962823,
      "cat": "ipc",
      "tid": 18951,
      "ph": "B"
    },
    {
      "name": "LayerTreeHostImpl::frameStateAsValue",
      "args": {},
      "pid": 4714,
      "ts": 1619172350269,
      "cat": "cc",
      "tid": 25131,
      "ph": "E"
    },
    {
      "name": "Frame",
      "args": {
        "frame": {
          "device_viewport_size": {
            "width": 1306.0,
            "height": 1242.0
          },
          "tiles": [
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "LOW_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 0.25,
              "id": "0x7a54cc90",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 245.0
                    },
                    "p3": {
                      "y": 447.0,
                      "x": 245.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 447.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "LOW_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "LOW_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a561110",
              "contents_scale": 0.25,
              "id": "0x7bb3dd50",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 1278.0
                    },
                    "p3": {
                      "y": 152.0,
                      "x": 1278.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 1274.0
                    },
                    "p4": {
                      "y": 152.0,
                      "x": 1274.0
                    }
                  },
                  "resolution": "LOW_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "LOW_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a541eb0",
              "contents_scale": 0.25,
              "id": "0x7bb3e330",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1210.0,
                      "x": 160.0
                    },
                    "p3": {
                      "y": 1214.0,
                      "x": 160.0
                    },
                    "p1": {
                      "y": 1210.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 1214.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "LOW_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a541eb0",
              "contents_scale": 2.0,
              "id": "0x7bb3e020",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1210.0,
                      "x": 511.0
                    },
                    "p3": {
                      "y": 1242.0,
                      "x": 511.0
                    },
                    "p1": {
                      "y": 1210.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 1242.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7cb46570",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 255.0
                    },
                    "p3": {
                      "y": 255.0,
                      "x": 255.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 255.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a561110",
              "contents_scale": 2.0,
              "id": "0x7bb3da30",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 1306.0
                    },
                    "p3": {
                      "y": 511.0,
                      "x": 1306.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 1274.0
                    },
                    "p4": {
                      "y": 511.0,
                      "x": 1274.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7cb44760",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 509.0
                    },
                    "p3": {
                      "y": 255.0,
                      "x": 509.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 255.0
                    },
                    "p4": {
                      "y": 255.0,
                      "x": 255.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7cb3c130",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 763.0
                    },
                    "p3": {
                      "y": 255.0,
                      "x": 763.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 509.0
                    },
                    "p4": {
                      "y": 255.0,
                      "x": 509.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a541eb0",
              "contents_scale": 2.0,
              "id": "0x7bb3e100",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1210.0,
                      "x": 1021.0
                    },
                    "p3": {
                      "y": 1242.0,
                      "x": 1021.0
                    },
                    "p1": {
                      "y": 1210.0,
                      "x": 511.0
                    },
                    "p4": {
                      "y": 1242.0,
                      "x": 511.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7a5474b0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 1017.0
                    },
                    "p3": {
                      "y": 255.0,
                      "x": 1017.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 763.0
                    },
                    "p4": {
                      "y": 255.0,
                      "x": 763.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7a546e80",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 1271.0
                    },
                    "p3": {
                      "y": 255.0,
                      "x": 1271.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 1017.0
                    },
                    "p4": {
                      "y": 255.0,
                      "x": 1017.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a541eb0",
              "contents_scale": 2.0,
              "id": "0x7bb3e1e0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1210.0,
                      "x": 1274.0
                    },
                    "p3": {
                      "y": 1242.0,
                      "x": 1274.0
                    },
                    "p1": {
                      "y": 1210.0,
                      "x": 1021.0
                    },
                    "p4": {
                      "y": 1242.0,
                      "x": 1021.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7cb4b710",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 1525.0
                    },
                    "p3": {
                      "y": 255.0,
                      "x": 1525.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 1271.0
                    },
                    "p4": {
                      "y": 255.0,
                      "x": 1271.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca520b0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 255.0,
                      "x": 255.0
                    },
                    "p3": {
                      "y": 509.0,
                      "x": 255.0
                    },
                    "p1": {
                      "y": 255.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 509.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca52190",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 255.0,
                      "x": 509.0
                    },
                    "p3": {
                      "y": 509.0,
                      "x": 509.0
                    },
                    "p1": {
                      "y": 255.0,
                      "x": 255.0
                    },
                    "p4": {
                      "y": 509.0,
                      "x": 255.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7a548930",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 255.0,
                      "x": 763.0
                    },
                    "p3": {
                      "y": 509.0,
                      "x": 763.0
                    },
                    "p1": {
                      "y": 255.0,
                      "x": 509.0
                    },
                    "p4": {
                      "y": 509.0,
                      "x": 509.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7a5605a0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 255.0,
                      "x": 1017.0
                    },
                    "p3": {
                      "y": 509.0,
                      "x": 1017.0
                    },
                    "p1": {
                      "y": 255.0,
                      "x": 763.0
                    },
                    "p4": {
                      "y": 509.0,
                      "x": 763.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7a561c80",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 255.0,
                      "x": 1271.0
                    },
                    "p3": {
                      "y": 509.0,
                      "x": 1271.0
                    },
                    "p1": {
                      "y": 255.0,
                      "x": 1017.0
                    },
                    "p4": {
                      "y": 509.0,
                      "x": 1017.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7a5552d0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 255.0,
                      "x": 1525.0
                    },
                    "p3": {
                      "y": 509.0,
                      "x": 1525.0
                    },
                    "p1": {
                      "y": 255.0,
                      "x": 1271.0
                    },
                    "p4": {
                      "y": 509.0,
                      "x": 1271.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca527b0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 509.0,
                      "x": 255.0
                    },
                    "p3": {
                      "y": 763.0,
                      "x": 255.0
                    },
                    "p1": {
                      "y": 509.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 763.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca52890",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 509.0,
                      "x": 509.0
                    },
                    "p3": {
                      "y": 763.0,
                      "x": 509.0
                    },
                    "p1": {
                      "y": 509.0,
                      "x": 255.0
                    },
                    "p4": {
                      "y": 763.0,
                      "x": 255.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca52970",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 509.0,
                      "x": 763.0
                    },
                    "p3": {
                      "y": 763.0,
                      "x": 763.0
                    },
                    "p1": {
                      "y": 509.0,
                      "x": 509.0
                    },
                    "p4": {
                      "y": 763.0,
                      "x": 509.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca52a50",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 509.0,
                      "x": 1017.0
                    },
                    "p3": {
                      "y": 763.0,
                      "x": 1017.0
                    },
                    "p1": {
                      "y": 509.0,
                      "x": 763.0
                    },
                    "p4": {
                      "y": 763.0,
                      "x": 763.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca52b40",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 509.0,
                      "x": 1271.0
                    },
                    "p3": {
                      "y": 763.0,
                      "x": 1271.0
                    },
                    "p1": {
                      "y": 509.0,
                      "x": 1017.0
                    },
                    "p4": {
                      "y": 763.0,
                      "x": 1017.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca52c30",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 509.0,
                      "x": 1525.0
                    },
                    "p3": {
                      "y": 763.0,
                      "x": 1525.0
                    },
                    "p1": {
                      "y": 509.0,
                      "x": 1271.0
                    },
                    "p4": {
                      "y": 763.0,
                      "x": 1271.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a561110",
              "contents_scale": 2.0,
              "id": "0x7bb3db10",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 511.0,
                      "x": 1306.0
                    },
                    "p3": {
                      "y": 1021.0,
                      "x": 1306.0
                    },
                    "p1": {
                      "y": 511.0,
                      "x": 1274.0
                    },
                    "p4": {
                      "y": 1021.0,
                      "x": 1274.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca52f00",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 763.0,
                      "x": 255.0
                    },
                    "p3": {
                      "y": 1017.0,
                      "x": 255.0
                    },
                    "p1": {
                      "y": 763.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 1017.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca52ff0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 763.0,
                      "x": 509.0
                    },
                    "p3": {
                      "y": 1017.0,
                      "x": 509.0
                    },
                    "p1": {
                      "y": 763.0,
                      "x": 255.0
                    },
                    "p4": {
                      "y": 1017.0,
                      "x": 255.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7a45b260",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 763.0,
                      "x": 763.0
                    },
                    "p3": {
                      "y": 1017.0,
                      "x": 763.0
                    },
                    "p1": {
                      "y": 763.0,
                      "x": 509.0
                    },
                    "p4": {
                      "y": 1017.0,
                      "x": 509.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca531d0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 763.0,
                      "x": 1017.0
                    },
                    "p3": {
                      "y": 1017.0,
                      "x": 1017.0
                    },
                    "p1": {
                      "y": 763.0,
                      "x": 763.0
                    },
                    "p4": {
                      "y": 1017.0,
                      "x": 763.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca532b0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 763.0,
                      "x": 1271.0
                    },
                    "p3": {
                      "y": 1017.0,
                      "x": 1271.0
                    },
                    "p1": {
                      "y": 763.0,
                      "x": 1017.0
                    },
                    "p4": {
                      "y": 1017.0,
                      "x": 1017.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7a467ea0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 763.0,
                      "x": 1525.0
                    },
                    "p3": {
                      "y": 1017.0,
                      "x": 1525.0
                    },
                    "p1": {
                      "y": 763.0,
                      "x": 1271.0
                    },
                    "p4": {
                      "y": 1017.0,
                      "x": 1271.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca53680",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1017.0,
                      "x": 255.0
                    },
                    "p3": {
                      "y": 1271.0,
                      "x": 255.0
                    },
                    "p1": {
                      "y": 1017.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 1271.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca53770",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1017.0,
                      "x": 509.0
                    },
                    "p3": {
                      "y": 1271.0,
                      "x": 509.0
                    },
                    "p1": {
                      "y": 1017.0,
                      "x": 255.0
                    },
                    "p4": {
                      "y": 1271.0,
                      "x": 255.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca53860",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1017.0,
                      "x": 763.0
                    },
                    "p3": {
                      "y": 1271.0,
                      "x": 763.0
                    },
                    "p1": {
                      "y": 1017.0,
                      "x": 509.0
                    },
                    "p4": {
                      "y": 1271.0,
                      "x": 509.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca53950",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1017.0,
                      "x": 1017.0
                    },
                    "p3": {
                      "y": 1271.0,
                      "x": 1017.0
                    },
                    "p1": {
                      "y": 1017.0,
                      "x": 763.0
                    },
                    "p4": {
                      "y": 1271.0,
                      "x": 763.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca61440",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1017.0,
                      "x": 1271.0
                    },
                    "p3": {
                      "y": 1271.0,
                      "x": 1271.0
                    },
                    "p1": {
                      "y": 1017.0,
                      "x": 1017.0
                    },
                    "p4": {
                      "y": 1271.0,
                      "x": 1017.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca53b30",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1017.0,
                      "x": 1525.0
                    },
                    "p3": {
                      "y": 1271.0,
                      "x": 1525.0
                    },
                    "p1": {
                      "y": 1017.0,
                      "x": 1271.0
                    },
                    "p4": {
                      "y": 1271.0,
                      "x": 1271.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a561110",
              "contents_scale": 2.0,
              "id": "0x7bb3dbf0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1021.0,
                      "x": 1306.0
                    },
                    "p3": {
                      "y": 1210.0,
                      "x": 1306.0
                    },
                    "p1": {
                      "y": 1021.0,
                      "x": 1274.0
                    },
                    "p4": {
                      "y": 1210.0,
                      "x": 1274.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 0.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7cb3d180",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 1779.0
                    },
                    "p3": {
                      "y": 255.0,
                      "x": 1779.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 1525.0
                    },
                    "p4": {
                      "y": 255.0,
                      "x": 1525.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 220.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca525f0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 255.0,
                      "x": 1779.0
                    },
                    "p3": {
                      "y": 509.0,
                      "x": 1779.0
                    },
                    "p1": {
                      "y": 255.0,
                      "x": 1525.0
                    },
                    "p4": {
                      "y": 509.0,
                      "x": 1525.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 220.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca52d20",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 509.0,
                      "x": 1779.0
                    },
                    "p3": {
                      "y": 763.0,
                      "x": 1779.0
                    },
                    "p1": {
                      "y": 509.0,
                      "x": 1525.0
                    },
                    "p4": {
                      "y": 763.0,
                      "x": 1525.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 220.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca534a0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 763.0,
                      "x": 1779.0
                    },
                    "p3": {
                      "y": 1017.0,
                      "x": 1779.0
                    },
                    "p1": {
                      "y": 763.0,
                      "x": 1525.0
                    },
                    "p4": {
                      "y": 1017.0,
                      "x": 1525.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 220.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca53c20",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1017.0,
                      "x": 1779.0
                    },
                    "p3": {
                      "y": 1271.0,
                      "x": 1779.0
                    },
                    "p1": {
                      "y": 1017.0,
                      "x": 1525.0
                    },
                    "p4": {
                      "y": 1271.0,
                      "x": 1525.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 220.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca53e00",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1271.0,
                      "x": 255.0
                    },
                    "p3": {
                      "y": 1525.0,
                      "x": 255.0
                    },
                    "p1": {
                      "y": 1271.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 1525.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 30.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca53ef0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1271.0,
                      "x": 509.0
                    },
                    "p3": {
                      "y": 1525.0,
                      "x": 509.0
                    },
                    "p1": {
                      "y": 1271.0,
                      "x": 255.0
                    },
                    "p4": {
                      "y": 1525.0,
                      "x": 255.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 30.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7a559fb0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1271.0,
                      "x": 763.0
                    },
                    "p3": {
                      "y": 1525.0,
                      "x": 763.0
                    },
                    "p1": {
                      "y": 1271.0,
                      "x": 509.0
                    },
                    "p4": {
                      "y": 1525.0,
                      "x": 509.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 30.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca540d0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1271.0,
                      "x": 1017.0
                    },
                    "p3": {
                      "y": 1525.0,
                      "x": 1017.0
                    },
                    "p1": {
                      "y": 1271.0,
                      "x": 763.0
                    },
                    "p4": {
                      "y": 1525.0,
                      "x": 763.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 30.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca541c0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1271.0,
                      "x": 1271.0
                    },
                    "p3": {
                      "y": 1525.0,
                      "x": 1271.0
                    },
                    "p1": {
                      "y": 1271.0,
                      "x": 1017.0
                    },
                    "p4": {
                      "y": 1525.0,
                      "x": 1017.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 30.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7a543e30",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1271.0,
                      "x": 1525.0
                    },
                    "p3": {
                      "y": 1525.0,
                      "x": 1525.0
                    },
                    "p1": {
                      "y": 1271.0,
                      "x": 1271.0
                    },
                    "p4": {
                      "y": 1525.0,
                      "x": 1271.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 30.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca543a0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1271.0,
                      "x": 1779.0
                    },
                    "p3": {
                      "y": 1525.0,
                      "x": 1779.0
                    },
                    "p1": {
                      "y": 1271.0,
                      "x": 1525.0
                    },
                    "p4": {
                      "y": 1525.0,
                      "x": 1525.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 250.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca54580",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1525.0,
                      "x": 255.0
                    },
                    "p3": {
                      "y": 1779.0,
                      "x": 255.0
                    },
                    "p1": {
                      "y": 1525.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 1779.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 284.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca54670",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1525.0,
                      "x": 509.0
                    },
                    "p3": {
                      "y": 1779.0,
                      "x": 509.0
                    },
                    "p1": {
                      "y": 1525.0,
                      "x": 255.0
                    },
                    "p4": {
                      "y": 1779.0,
                      "x": 255.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 284.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7a54aab0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1525.0,
                      "x": 763.0
                    },
                    "p3": {
                      "y": 1779.0,
                      "x": 763.0
                    },
                    "p1": {
                      "y": 1525.0,
                      "x": 509.0
                    },
                    "p4": {
                      "y": 1779.0,
                      "x": 509.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 284.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca54850",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1525.0,
                      "x": 1017.0
                    },
                    "p3": {
                      "y": 1779.0,
                      "x": 1017.0
                    },
                    "p1": {
                      "y": 1525.0,
                      "x": 763.0
                    },
                    "p4": {
                      "y": 1779.0,
                      "x": 763.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 284.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca54940",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1525.0,
                      "x": 1271.0
                    },
                    "p3": {
                      "y": 1779.0,
                      "x": 1271.0
                    },
                    "p1": {
                      "y": 1525.0,
                      "x": 1017.0
                    },
                    "p4": {
                      "y": 1779.0,
                      "x": 1017.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 284.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "NOW_BIN",
                  "0": "NOW_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "NOW_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7a5601a0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1525.0,
                      "x": 1525.0
                    },
                    "p3": {
                      "y": 1779.0,
                      "x": 1525.0
                    },
                    "p1": {
                      "y": 1525.0,
                      "x": 1271.0
                    },
                    "p4": {
                      "y": 1779.0,
                      "x": 1271.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 284.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca51fd0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 1960.0
                    },
                    "p3": {
                      "y": 255.0,
                      "x": 1960.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 1779.0
                    },
                    "p4": {
                      "y": 255.0,
                      "x": 1779.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 474.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca526d0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 255.0,
                      "x": 1960.0
                    },
                    "p3": {
                      "y": 509.0,
                      "x": 1960.0
                    },
                    "p1": {
                      "y": 255.0,
                      "x": 1779.0
                    },
                    "p4": {
                      "y": 509.0,
                      "x": 1779.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 474.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca52e10",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 509.0,
                      "x": 1960.0
                    },
                    "p3": {
                      "y": 763.0,
                      "x": 1960.0
                    },
                    "p1": {
                      "y": 509.0,
                      "x": 1779.0
                    },
                    "p4": {
                      "y": 763.0,
                      "x": 1779.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 474.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca53590",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 763.0,
                      "x": 1960.0
                    },
                    "p3": {
                      "y": 1017.0,
                      "x": 1960.0
                    },
                    "p1": {
                      "y": 763.0,
                      "x": 1779.0
                    },
                    "p4": {
                      "y": 1017.0,
                      "x": 1779.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 474.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca53d10",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1017.0,
                      "x": 1960.0
                    },
                    "p3": {
                      "y": 1271.0,
                      "x": 1960.0
                    },
                    "p1": {
                      "y": 1017.0,
                      "x": 1779.0
                    },
                    "p4": {
                      "y": 1271.0,
                      "x": 1779.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 474.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca54490",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1271.0,
                      "x": 1960.0
                    },
                    "p3": {
                      "y": 1525.0,
                      "x": 1960.0
                    },
                    "p1": {
                      "y": 1271.0,
                      "x": 1779.0
                    },
                    "p4": {
                      "y": 1525.0,
                      "x": 1779.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 504.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca54b20",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1525.0,
                      "x": 1779.0
                    },
                    "p3": {
                      "y": 1779.0,
                      "x": 1779.0
                    },
                    "p1": {
                      "y": 1525.0,
                      "x": 1525.0
                    },
                    "p4": {
                      "y": 1779.0,
                      "x": 1525.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 504.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca54c10",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1525.0,
                      "x": 1960.0
                    },
                    "p3": {
                      "y": 1779.0,
                      "x": 1960.0
                    },
                    "p1": {
                      "y": 1525.0,
                      "x": 1779.0
                    },
                    "p4": {
                      "y": 1779.0,
                      "x": 1779.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 758.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca54d00",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1779.0,
                      "x": 255.0
                    },
                    "p3": {
                      "y": 2033.0,
                      "x": 255.0
                    },
                    "p1": {
                      "y": 1779.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 2033.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 538.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7cb47db0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1779.0,
                      "x": 509.0
                    },
                    "p3": {
                      "y": 2033.0,
                      "x": 509.0
                    },
                    "p1": {
                      "y": 1779.0,
                      "x": 255.0
                    },
                    "p4": {
                      "y": 2033.0,
                      "x": 255.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 538.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7cb474e0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1779.0,
                      "x": 763.0
                    },
                    "p3": {
                      "y": 2033.0,
                      "x": 763.0
                    },
                    "p1": {
                      "y": 1779.0,
                      "x": 509.0
                    },
                    "p4": {
                      "y": 2033.0,
                      "x": 509.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 538.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca54fd0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1779.0,
                      "x": 1017.0
                    },
                    "p3": {
                      "y": 2033.0,
                      "x": 1017.0
                    },
                    "p1": {
                      "y": 1779.0,
                      "x": 763.0
                    },
                    "p4": {
                      "y": 2033.0,
                      "x": 763.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 538.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca550c0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1779.0,
                      "x": 1271.0
                    },
                    "p3": {
                      "y": 2033.0,
                      "x": 1271.0
                    },
                    "p1": {
                      "y": 1779.0,
                      "x": 1017.0
                    },
                    "p4": {
                      "y": 2033.0,
                      "x": 1017.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 538.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7cb49fc0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1779.0,
                      "x": 1525.0
                    },
                    "p3": {
                      "y": 2033.0,
                      "x": 1525.0
                    },
                    "p1": {
                      "y": 1779.0,
                      "x": 1271.0
                    },
                    "p4": {
                      "y": 2033.0,
                      "x": 1271.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 538.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca552a0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1779.0,
                      "x": 1779.0
                    },
                    "p3": {
                      "y": 2033.0,
                      "x": 1779.0
                    },
                    "p1": {
                      "y": 1779.0,
                      "x": 1525.0
                    },
                    "p4": {
                      "y": 2033.0,
                      "x": 1525.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 758.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca55390",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 1779.0,
                      "x": 1960.0
                    },
                    "p3": {
                      "y": 2033.0,
                      "x": 1960.0
                    },
                    "p1": {
                      "y": 1779.0,
                      "x": 1779.0
                    },
                    "p4": {
                      "y": 2033.0,
                      "x": 1779.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1012.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca55480",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2033.0,
                      "x": 255.0
                    },
                    "p3": {
                      "y": 2287.0,
                      "x": 255.0
                    },
                    "p1": {
                      "y": 2033.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 2287.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 792.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7a5cd170",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2033.0,
                      "x": 509.0
                    },
                    "p3": {
                      "y": 2287.0,
                      "x": 509.0
                    },
                    "p1": {
                      "y": 2033.0,
                      "x": 255.0
                    },
                    "p4": {
                      "y": 2287.0,
                      "x": 255.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 792.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7a550250",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2033.0,
                      "x": 763.0
                    },
                    "p3": {
                      "y": 2287.0,
                      "x": 763.0
                    },
                    "p1": {
                      "y": 2033.0,
                      "x": 509.0
                    },
                    "p4": {
                      "y": 2287.0,
                      "x": 509.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 792.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7a45c3b0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2033.0,
                      "x": 1017.0
                    },
                    "p3": {
                      "y": 2287.0,
                      "x": 1017.0
                    },
                    "p1": {
                      "y": 2033.0,
                      "x": 763.0
                    },
                    "p4": {
                      "y": 2287.0,
                      "x": 763.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 792.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca55840",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2033.0,
                      "x": 1271.0
                    },
                    "p3": {
                      "y": 2287.0,
                      "x": 1271.0
                    },
                    "p1": {
                      "y": 2033.0,
                      "x": 1017.0
                    },
                    "p4": {
                      "y": 2287.0,
                      "x": 1017.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 792.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7a550330",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2033.0,
                      "x": 1525.0
                    },
                    "p3": {
                      "y": 2287.0,
                      "x": 1525.0
                    },
                    "p1": {
                      "y": 2033.0,
                      "x": 1271.0
                    },
                    "p4": {
                      "y": 2287.0,
                      "x": 1271.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 792.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca55a20",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2033.0,
                      "x": 1779.0
                    },
                    "p3": {
                      "y": 2287.0,
                      "x": 1779.0
                    },
                    "p1": {
                      "y": 2033.0,
                      "x": 1525.0
                    },
                    "p4": {
                      "y": 2287.0,
                      "x": 1525.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1012.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca55b10",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2033.0,
                      "x": 1960.0
                    },
                    "p3": {
                      "y": 2287.0,
                      "x": 1960.0
                    },
                    "p1": {
                      "y": 2033.0,
                      "x": 1779.0
                    },
                    "p4": {
                      "y": 2287.0,
                      "x": 1779.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1266.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca55c00",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2287.0,
                      "x": 255.0
                    },
                    "p3": {
                      "y": 2541.0,
                      "x": 255.0
                    },
                    "p1": {
                      "y": 2287.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 2541.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1046.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca55cf0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2287.0,
                      "x": 509.0
                    },
                    "p3": {
                      "y": 2541.0,
                      "x": 509.0
                    },
                    "p1": {
                      "y": 2287.0,
                      "x": 255.0
                    },
                    "p4": {
                      "y": 2541.0,
                      "x": 255.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1046.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca55de0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2287.0,
                      "x": 763.0
                    },
                    "p3": {
                      "y": 2541.0,
                      "x": 763.0
                    },
                    "p1": {
                      "y": 2287.0,
                      "x": 509.0
                    },
                    "p4": {
                      "y": 2541.0,
                      "x": 509.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1046.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca55ed0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2287.0,
                      "x": 1017.0
                    },
                    "p3": {
                      "y": 2541.0,
                      "x": 1017.0
                    },
                    "p1": {
                      "y": 2287.0,
                      "x": 763.0
                    },
                    "p4": {
                      "y": 2541.0,
                      "x": 763.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1046.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca55fc0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2287.0,
                      "x": 1271.0
                    },
                    "p3": {
                      "y": 2541.0,
                      "x": 1271.0
                    },
                    "p1": {
                      "y": 2287.0,
                      "x": 1017.0
                    },
                    "p4": {
                      "y": 2541.0,
                      "x": 1017.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1046.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca560b0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2287.0,
                      "x": 1525.0
                    },
                    "p3": {
                      "y": 2541.0,
                      "x": 1525.0
                    },
                    "p1": {
                      "y": 2287.0,
                      "x": 1271.0
                    },
                    "p4": {
                      "y": 2541.0,
                      "x": 1271.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1046.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca561a0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2287.0,
                      "x": 1779.0
                    },
                    "p3": {
                      "y": 2541.0,
                      "x": 1779.0
                    },
                    "p1": {
                      "y": 2287.0,
                      "x": 1525.0
                    },
                    "p4": {
                      "y": 2541.0,
                      "x": 1525.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1266.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca56290",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2287.0,
                      "x": 1960.0
                    },
                    "p3": {
                      "y": 2541.0,
                      "x": 1960.0
                    },
                    "p1": {
                      "y": 2287.0,
                      "x": 1779.0
                    },
                    "p4": {
                      "y": 2541.0,
                      "x": 1779.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1520.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca56380",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2541.0,
                      "x": 255.0
                    },
                    "p3": {
                      "y": 2795.0,
                      "x": 255.0
                    },
                    "p1": {
                      "y": 2541.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 2795.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1300.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca56470",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2541.0,
                      "x": 509.0
                    },
                    "p3": {
                      "y": 2795.0,
                      "x": 509.0
                    },
                    "p1": {
                      "y": 2541.0,
                      "x": 255.0
                    },
                    "p4": {
                      "y": 2795.0,
                      "x": 255.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1300.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca56560",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2541.0,
                      "x": 763.0
                    },
                    "p3": {
                      "y": 2795.0,
                      "x": 763.0
                    },
                    "p1": {
                      "y": 2541.0,
                      "x": 509.0
                    },
                    "p4": {
                      "y": 2795.0,
                      "x": 509.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1300.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca56650",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2541.0,
                      "x": 1017.0
                    },
                    "p3": {
                      "y": 2795.0,
                      "x": 1017.0
                    },
                    "p1": {
                      "y": 2541.0,
                      "x": 763.0
                    },
                    "p4": {
                      "y": 2795.0,
                      "x": 763.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1300.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca56740",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2541.0,
                      "x": 1271.0
                    },
                    "p3": {
                      "y": 2795.0,
                      "x": 1271.0
                    },
                    "p1": {
                      "y": 2541.0,
                      "x": 1017.0
                    },
                    "p4": {
                      "y": 2795.0,
                      "x": 1017.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1300.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca56830",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2541.0,
                      "x": 1525.0
                    },
                    "p3": {
                      "y": 2795.0,
                      "x": 1525.0
                    },
                    "p1": {
                      "y": 2541.0,
                      "x": 1271.0
                    },
                    "p4": {
                      "y": 2795.0,
                      "x": 1271.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1300.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca56920",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2541.0,
                      "x": 1779.0
                    },
                    "p3": {
                      "y": 2795.0,
                      "x": 1779.0
                    },
                    "p1": {
                      "y": 2541.0,
                      "x": 1525.0
                    },
                    "p4": {
                      "y": 2795.0,
                      "x": 1525.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1520.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca56a10",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2541.0,
                      "x": 1960.0
                    },
                    "p3": {
                      "y": 2795.0,
                      "x": 1960.0
                    },
                    "p1": {
                      "y": 2541.0,
                      "x": 1779.0
                    },
                    "p4": {
                      "y": 2795.0,
                      "x": 1779.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1774.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca56b00",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2795.0,
                      "x": 255.0
                    },
                    "p3": {
                      "y": 3049.0,
                      "x": 255.0
                    },
                    "p1": {
                      "y": 2795.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 3049.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1554.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca56bf0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2795.0,
                      "x": 509.0
                    },
                    "p3": {
                      "y": 3049.0,
                      "x": 509.0
                    },
                    "p1": {
                      "y": 2795.0,
                      "x": 255.0
                    },
                    "p4": {
                      "y": 3049.0,
                      "x": 255.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1554.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca56ce0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2795.0,
                      "x": 763.0
                    },
                    "p3": {
                      "y": 3049.0,
                      "x": 763.0
                    },
                    "p1": {
                      "y": 2795.0,
                      "x": 509.0
                    },
                    "p4": {
                      "y": 3049.0,
                      "x": 509.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1554.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca56dd0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2795.0,
                      "x": 1017.0
                    },
                    "p3": {
                      "y": 3049.0,
                      "x": 1017.0
                    },
                    "p1": {
                      "y": 2795.0,
                      "x": 763.0
                    },
                    "p4": {
                      "y": 3049.0,
                      "x": 763.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1554.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca56ec0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2795.0,
                      "x": 1271.0
                    },
                    "p3": {
                      "y": 3049.0,
                      "x": 1271.0
                    },
                    "p1": {
                      "y": 2795.0,
                      "x": 1017.0
                    },
                    "p4": {
                      "y": 3049.0,
                      "x": 1017.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1554.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca56fb0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2795.0,
                      "x": 1525.0
                    },
                    "p3": {
                      "y": 3049.0,
                      "x": 1525.0
                    },
                    "p1": {
                      "y": 2795.0,
                      "x": 1271.0
                    },
                    "p4": {
                      "y": 3049.0,
                      "x": 1271.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1554.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca570a0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2795.0,
                      "x": 1779.0
                    },
                    "p3": {
                      "y": 3049.0,
                      "x": 1779.0
                    },
                    "p1": {
                      "y": 2795.0,
                      "x": 1525.0
                    },
                    "p4": {
                      "y": 3049.0,
                      "x": 1525.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1774.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca57190",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 2795.0,
                      "x": 1960.0
                    },
                    "p3": {
                      "y": 3049.0,
                      "x": 1960.0
                    },
                    "p1": {
                      "y": 2795.0,
                      "x": 1779.0
                    },
                    "p4": {
                      "y": 3049.0,
                      "x": 1779.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 2028.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca57280",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3049.0,
                      "x": 255.0
                    },
                    "p3": {
                      "y": 3303.0,
                      "x": 255.0
                    },
                    "p1": {
                      "y": 3049.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 3303.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1808.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca57370",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3049.0,
                      "x": 509.0
                    },
                    "p3": {
                      "y": 3303.0,
                      "x": 509.0
                    },
                    "p1": {
                      "y": 3049.0,
                      "x": 255.0
                    },
                    "p4": {
                      "y": 3303.0,
                      "x": 255.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1808.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca57460",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3049.0,
                      "x": 763.0
                    },
                    "p3": {
                      "y": 3303.0,
                      "x": 763.0
                    },
                    "p1": {
                      "y": 3049.0,
                      "x": 509.0
                    },
                    "p4": {
                      "y": 3303.0,
                      "x": 509.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1808.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca57550",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3049.0,
                      "x": 1017.0
                    },
                    "p3": {
                      "y": 3303.0,
                      "x": 1017.0
                    },
                    "p1": {
                      "y": 3049.0,
                      "x": 763.0
                    },
                    "p4": {
                      "y": 3303.0,
                      "x": 763.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1808.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca57640",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3049.0,
                      "x": 1271.0
                    },
                    "p3": {
                      "y": 3303.0,
                      "x": 1271.0
                    },
                    "p1": {
                      "y": 3049.0,
                      "x": 1017.0
                    },
                    "p4": {
                      "y": 3303.0,
                      "x": 1017.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1808.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca57730",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3049.0,
                      "x": 1525.0
                    },
                    "p3": {
                      "y": 3303.0,
                      "x": 1525.0
                    },
                    "p1": {
                      "y": 3049.0,
                      "x": 1271.0
                    },
                    "p4": {
                      "y": 3303.0,
                      "x": 1271.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 1808.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca57820",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3049.0,
                      "x": 1779.0
                    },
                    "p3": {
                      "y": 3303.0,
                      "x": 1779.0
                    },
                    "p1": {
                      "y": 3049.0,
                      "x": 1525.0
                    },
                    "p4": {
                      "y": 3303.0,
                      "x": 1525.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 2028.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca57910",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3049.0,
                      "x": 1960.0
                    },
                    "p3": {
                      "y": 3303.0,
                      "x": 1960.0
                    },
                    "p1": {
                      "y": 3049.0,
                      "x": 1779.0
                    },
                    "p4": {
                      "y": 3303.0,
                      "x": 1779.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 2282.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca57a00",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3303.0,
                      "x": 255.0
                    },
                    "p3": {
                      "y": 3557.0,
                      "x": 255.0
                    },
                    "p1": {
                      "y": 3303.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 3557.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 2062.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca57af0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3303.0,
                      "x": 509.0
                    },
                    "p3": {
                      "y": 3557.0,
                      "x": 509.0
                    },
                    "p1": {
                      "y": 3303.0,
                      "x": 255.0
                    },
                    "p4": {
                      "y": 3557.0,
                      "x": 255.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 2062.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca57be0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3303.0,
                      "x": 763.0
                    },
                    "p3": {
                      "y": 3557.0,
                      "x": 763.0
                    },
                    "p1": {
                      "y": 3303.0,
                      "x": 509.0
                    },
                    "p4": {
                      "y": 3557.0,
                      "x": 509.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 2062.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca57cd0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3303.0,
                      "x": 1017.0
                    },
                    "p3": {
                      "y": 3557.0,
                      "x": 1017.0
                    },
                    "p1": {
                      "y": 3303.0,
                      "x": 763.0
                    },
                    "p4": {
                      "y": 3557.0,
                      "x": 763.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 2062.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca57dc0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3303.0,
                      "x": 1271.0
                    },
                    "p3": {
                      "y": 3557.0,
                      "x": 1271.0
                    },
                    "p1": {
                      "y": 3303.0,
                      "x": 1017.0
                    },
                    "p4": {
                      "y": 3557.0,
                      "x": 1017.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 2062.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca57eb0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3303.0,
                      "x": 1525.0
                    },
                    "p3": {
                      "y": 3557.0,
                      "x": 1525.0
                    },
                    "p1": {
                      "y": 3303.0,
                      "x": 1271.0
                    },
                    "p4": {
                      "y": 3557.0,
                      "x": 1271.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 2062.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca57fa0",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3303.0,
                      "x": 1779.0
                    },
                    "p3": {
                      "y": 3557.0,
                      "x": 1779.0
                    },
                    "p1": {
                      "y": 3303.0,
                      "x": 1525.0
                    },
                    "p4": {
                      "y": 3557.0,
                      "x": 1525.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 2282.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca58090",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3303.0,
                      "x": 1960.0
                    },
                    "p3": {
                      "y": 3557.0,
                      "x": 1960.0
                    },
                    "p1": {
                      "y": 3303.0,
                      "x": 1779.0
                    },
                    "p4": {
                      "y": 3557.0,
                      "x": 1779.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 2536.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca58180",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3557.0,
                      "x": 255.0
                    },
                    "p3": {
                      "y": 3570.0,
                      "x": 255.0
                    },
                    "p1": {
                      "y": 3557.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 3570.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 2316.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca58270",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3557.0,
                      "x": 509.0
                    },
                    "p3": {
                      "y": 3570.0,
                      "x": 509.0
                    },
                    "p1": {
                      "y": 3557.0,
                      "x": 255.0
                    },
                    "p4": {
                      "y": 3570.0,
                      "x": 255.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 2316.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca58360",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3557.0,
                      "x": 763.0
                    },
                    "p3": {
                      "y": 3570.0,
                      "x": 763.0
                    },
                    "p1": {
                      "y": 3557.0,
                      "x": 509.0
                    },
                    "p4": {
                      "y": 3570.0,
                      "x": 509.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 2316.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca58450",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3557.0,
                      "x": 1017.0
                    },
                    "p3": {
                      "y": 3570.0,
                      "x": 1017.0
                    },
                    "p1": {
                      "y": 3557.0,
                      "x": 763.0
                    },
                    "p4": {
                      "y": 3570.0,
                      "x": 763.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 2316.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca58540",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3557.0,
                      "x": 1271.0
                    },
                    "p3": {
                      "y": 3570.0,
                      "x": 1271.0
                    },
                    "p1": {
                      "y": 3557.0,
                      "x": 1017.0
                    },
                    "p4": {
                      "y": 3570.0,
                      "x": 1017.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 2316.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca58630",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3557.0,
                      "x": 1525.0
                    },
                    "p3": {
                      "y": 3570.0,
                      "x": 1525.0
                    },
                    "p1": {
                      "y": 3557.0,
                      "x": 1271.0
                    },
                    "p4": {
                      "y": 3570.0,
                      "x": 1271.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 2316.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca58720",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3557.0,
                      "x": 1779.0
                    },
                    "p3": {
                      "y": 3570.0,
                      "x": 1779.0
                    },
                    "p1": {
                      "y": 3557.0,
                      "x": 1525.0
                    },
                    "p4": {
                      "y": 3570.0,
                      "x": 1525.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 2536.0
                }
              }
            },
            {
              "managed_state": {
                "bin": {
                  "1": "EVENTUALLY_BIN",
                  "0": "EVENTUALLY_BIN"
                },
                "can_be_freed": true,
                "resolution": "HIGH_RESOLUTION",
                "can_use_gpu_memory": true,
                "time_to_needed_in_seconds": 0.0,
                "resource_is_being_initialized": false,
                "has_resource": true,
                "gpu_memmgr_stats_bin": "EVENTUALLY_BIN",
                "raster_state": "IDLE_STATE"
              },
              "picture_pile": "0x7a5560f0",
              "contents_scale": 2.0,
              "id": "0x7ca58810",
              "priority": {
                "1": {
                  "time_to_visible_in_seconds": 3.4028234663852886e+38,
                  "is_live": false,
                  "current_screen_quad": {
                    "p2": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p3": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p1": {
                      "y": 0.0,
                      "x": 0.0
                    },
                    "p4": {
                      "y": 0.0,
                      "x": 0.0
                    }
                  },
                  "resolution": "<unknown TileResolution value>",
                  "distance_to_visible_in_pixels": 3.4028234663852886e+38
                },
                "0": {
                  "time_to_visible_in_seconds": 0.0,
                  "is_live": true,
                  "current_screen_quad": {
                    "p2": {
                      "y": 3557.0,
                      "x": 1960.0
                    },
                    "p3": {
                      "y": 3570.0,
                      "x": 1960.0
                    },
                    "p1": {
                      "y": 3557.0,
                      "x": 1779.0
                    },
                    "p4": {
                      "y": 3570.0,
                      "x": 1779.0
                    }
                  },
                  "resolution": "HIGH_RESOLUTION",
                  "distance_to_visible_in_pixels": 2790.0
                }
              }
            }
          ],
          "compositor_instance": "0x79671840"
        }
      },
      "pid": 73613,
      "ts": 86651388628,
      "cat": "cc.debug",
      "tid": 25131,
      "ph": "I"
    }
  ]
}
