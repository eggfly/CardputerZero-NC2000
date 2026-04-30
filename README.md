# CardputerZero-NC2000

文曲星 **NC2000** 模拟器，针对 **M5CardputerZero**（320×170 LCD）适配，作为第一个 SDL2 的 CardputerZero App 示例。

基于 [wangyu-/NC2000](https://github.com/wangyu-/NC2000) 上游源码（GPL v3，版权归原作者），本仓库新增：

- 320×170 居中显示参数
- APPLauncher `.desktop` 集成
- `build_deb.sh` 一键打包
- GitHub Actions 交叉构建 arm64 `.deb`

![icon](packaging/nc2000_icon.png)

## 特性

- 运行真机 dump 的 ROM，兼容各种自制内核
- 4 灰度 LCD + 侧边小图标（lcdstripe）
- 声音（蜂鸣器 / DSP）
- 即时存档、快进、超频
- 通过 APPLauncher 的 `.desktop` 入口启动
- CI 自动构建 arm64 `.deb`，装到 CardputerZero 即可

## ROM

本仓库在 `roms/` 里带了 **NC2000 官方 3.5** 的 ROM（真机 dump）：

- `nc2000.nand` — 32 MiB NAND flash
- `nc2000.nand0` — NAND OOB（boot 区）
- `nc2000.nor` — 512 KiB NOR flash

默认启动就是这套。想跑其他机型（nc2600 / nc1020 等）参考
[上游 wiki](https://github.com/wangyu-/NC2000/wiki/%E5%88%87%E6%8D%A2%E4%B8%8D%E5%90%8C%E6%9C%BA%E5%9E%8B%E5%92%8C%E5%86%85%E6%A0%B8)
自行准备对应文件放进 `roms/`，然后：

```sh
./cardputer/nc2000-launch.sh --nc1020 --rom /usr/share/nc2000/roms/nc1020
```

## 安装

### 从 Release 装 `.deb`（推荐）

从 [Releases](../../releases) 下载 `nc2000_<ver>_arm64.deb`，在 CardputerZero 上：

```sh
sudo dpkg -i nc2000_1.0.0_arm64.deb
sudo apt-get install -f   # 补全依赖（libsdl2-2.0-0）
```

APPLauncher 里就会出现 NC2000。

### 本地构建 `.deb`

需要 `cmake ≥ 3.15`、`g++`、`libsdl2-dev`、`pkg-config`、`fakeroot`：

```sh
sudo apt-get install -y build-essential cmake pkg-config fakeroot libsdl2-dev dpkg-dev
./packaging/build_deb.sh
# 产物：build/nc2000_1.0.0_arm64.deb
```

### 本地只编译可执行文件

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
# 运行（当前目录需要 roms/ 和 resource/）
./build/nc2000 --pixel-size 1 --gap-size 0 --lcd-scale 1
```

## CardputerZero 键位

CardputerZero 键盘已映射为标准 SDL keycode，上游键位基本直接可用。常用：

| 功能 | NC 按键 | CardputerZero |
|---|---|---|
| 英汉 / 名片 / 计算 / 行程 / 测验 / 时间 / 网络 | F5–F11 | `F5`–`F11`（`Fn + 1..7`） |
| on/off | F12 | `F12`（`Fn + 8`） |
| 发音 | `;` | `;` |
| 报时 | `'` | `'` |
| 求助 / 中英数 / 输入法 | `[` / `]` / `\` | 同键 |
| 红外 | Alt | `Alt` |
| 跳出 | Esc | `ESC` |
| 翻页上 / 下 | `,` / `?` | 同键 |
| 快进 | `TAB` | `TAB` |
| 内置命令行 | `` ` `` | `` ` `` |

完整键位见 [上游 Wiki](https://github.com/wangyu-/NC2000/wiki/%E6%A8%A1%E6%8B%9F%E5%99%A8%E9%94%AE%E4%BD%8D)。

## 屏幕适配

NC2000 原生 LCD 是 160×80 + 左 21px / 右 7px 图标条。上游默认 `pixel-size=4 gap-size=1`
窗口约 1040×400，在 CardputerZero 的 320×170 LCD 上显示不下。

`cardputer/nc2000-launch.sh` 强制：

```
--pixel-size 1 --gap-size 0 --lcd-scale 1
```

→ 窗口 187×80，在 320×170 上居中留黑边。想调整可改脚本或传参覆盖。

## 仓库结构

```
CardputerZero-NC2000/
├── *.cpp *.h          # NC2000 上游模拟器源码
├── ansi/ compare/ dsp/ lcdstripe/ misc/ test/  # 上游子目录
├── resource/          # LCD 侧边图标素材（上游）
├── roms/              # NC2000 3.5 ROM（nand/nand0/nor）
├── port_fb/           # 上游的 framebuffer-only port（未用）
├── cardputer/
│   ├── nc2000-launch.sh   # CardputerZero 启动脚本
│   └── nc2000.desktop     # APPLauncher 集成
├── packaging/
│   ├── build_deb.sh       # 构建 .deb
│   ├── nc2000_icon.png
│   └── VERSION
├── CMakeLists.txt
└── .github/workflows/
    └── build-deb.yml      # arm64 CI
```

## Credits

- **上游代码**: [wangyu-/NC2000](https://github.com/wangyu-/NC2000) — GPL v3
- **ROM**: 文曲星 NC2000 原厂固件
- **CardputerZero 适配**: [eggfly](https://github.com/eggfly)

## License

GPL v3（继承上游），详见 [LICENSE](LICENSE)。
