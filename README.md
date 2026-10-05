# I.MX6ULL Linux 驱动学习记录

记录我在 I.MX6ULL（ARM Cortex-A7）平台上学习 Linux 字符设备驱动的过程，
包含驱动源码、测试用的用户态 APP 以及每章的 Makefile。

## 开发环境

| 项目 | 说明 |
|------|------|
| 开发板 | 正点原子 I.MX6U-ALPHA（I.MX6ULL） |
| 内核版本 | Linux 4.1.15 |
| 交叉编译器 | `arm-linux-gnueabihf-gcc` |
| 开发方式 | Ubuntu + NFS 挂载 rootfs，模块动态加载（`insmod` / `rmmod`） |
| 调试串口 | `console=ttymxc0,115200` |

## 目录索引

| 目录 | 内容 | 状态 |
|------|------|------|
| `1_chrdevbase/` | 字符设备基础：手动分配设备号 + `register_chrdev` | ✅ |
| `2_led/` | LED 驱动：`ioremap` 寄存器映射、GPIO 输出控制 | ✅ |
| `3_newchrled/` | 新字符设备驱动：`alloc_chrdev_region` + `cdev` + `class_create` 自动建节点 | ✅ |

## 编译与部署

### 1. 编译内核模块

进入某个驱动目录后执行：

```bash
make
```

生成的 `.ko` 文件即内核模块。

### 2. 编译用户态测试 APP

```bash
arm-linux-gnueabihf-gcc ledAPP.c -o ledAPP
```

### 3. 部署到开发板（NFS rootfs）

```bash
sudo cp led.ko   /path/to/nfs/rootfs/lib/modules/4.1.15/ -f
sudo cp ledAPP   /path/to/nfs/rootfs/lib/modules/4.1.15/ -f
```

### 4. 在开发板上测试

```bash
insmod led.ko                 # 加载驱动
mknod /dev/led c 200 0        # 创建设备节点（主设备号 200）
./ledAPP /dev/led 1           # 开灯
./ledAPP /dev/led 0           # 关灯
rmmod led.ko                  # 卸载驱动
```

## 学习笔记

### 2_led —— LED 驱动

- **寄存器物理地址 → 虚拟地址**：内核不能直接访问物理地址，必须用
  `ioremap()` 建立映射，返回值类型为 `void __iomem *`。
- **`__iomem` 是什么**：`linux/compiler.h` 中定义的宏。正常 gcc 编译时展开为空，
  仅在 `sparse` 静态检查时展开为 `__attribute__((noderef, address_space(2)))`，
  用来标记"这块内存是 I/O 寄存器，不要直接解引用，要用 `readl`/`writel`"。
- **寄存器读写**：`readl()` / `writel()`，需要 `#include <linux/io.h>`。
- **应用与驱动的数据约定**：APP 用 `atoi()` 把命令行字符串转成数值 0/1，
  再 `write(fd, databuf, 1)` 写 **1 个字节**（`0x00` / `0x01`）。
  `char` / `unsigned char` 在 C 里是**整数类型**，存的是数值而非 ASCII 码。
- **`copy_from_user`** 返回**未成功拷贝的字节数**（成功为 0），不是负数。

#### LED 硬件信息

| 寄存器 | 物理地址 | 作用 |
|--------|----------|------|
| `CCM_CCGR1` | `0x020C406C` | 时钟门控（bit 26~27 使能 GPIO1） |
| `SW_MUX_GPIO1_IO03` | `0x020E0068` | 引脚复用（写 0x5 → GPIO 模式） |
| `SW_PAD_GPIO1_IO03` | `0x020E02F4` | 电气属性（写 0x10B0） |
| `GPIO1_GDIR` | `0x0209C004` | 方向寄存器（bit 3 = 1 → 输出） |
| `GPIO1_DR` | `0x0209C000` | 数据寄存器（bit 3 = 0 → 亮，1 → 灭） |

### 3_newchrled —— 新字符设备驱动

相对 `2_led` 用 `register_chrdev()` 的"老框架"，这里改用**新框架**，注册一个字符设备被拆成三步：

1. **申请设备号**：`alloc_chrdev_region()`（内核动态分配）或 `register_chrdev_region()`（指定号）
2. **注册设备**：`cdev_init()` + `cdev_add()`
3. **自动创建设备节点**：`class_create()` + `device_create()`（配合 udev/mdev）

要点笔记：

- **为什么要换新框架**：`register_chrdev()` 会一次性霸占整个主设备号下的所有次设备号，
  而且**不会自动创建 `/dev` 节点**，必须手动 `mknod`。新框架把这几件事解耦了。
- **设备号**：`MKDEV(major, minor)` 合成，`MAJOR()` / `MINOR()` 拆解。
- **`struct cdev`** 是内核对字符设备的抽象，只有执行完 `cdev_add()` 设备才真正"生效"。
- **错误回滚**：初始化过程用 `goto` **逐级回滚**（`fail_device_create` → `fail_class_create`
  → `fail_cdev_init` → `fail_register_chrdev_region`），避免中途失败留下半初始化状态。
  注意销毁顺序要**与创建顺序相反**，且 `device_destroy()` 必须在 `class_destroy()` 之前。
- **`IS_ERR()` / `PTR_ERR()`**：`class_create()` / `device_create()` 失败时返回的是**错误指针**
  （不是 `NULL`），必须用 `IS_ERR()` 判断、`PTR_ERR()` 取出错误码。
- **测试方式**：不用再 `mknod`，`insmod newchrled.ko` 后 `/dev/newchrled` 会自动出现。

```bash
insmod newchrled.ko                  # 加载，dmesg 里会打印分配到的 major/minor
./ledAPP /dev/newchrled 1            # 开灯
./ledAPP /dev/newchrled 0            # 关灯
rmmod newchrled                     # 卸载
```

> 注：`newchrled_release()` 里的局部变量 `dev` 目前没被使用，编译会有
> `-Wunused-variable` 警告（不影响功能），后续可删掉或改用 `private_data`。

## 约定

- 只提交源码（`*.c`、`Makefile`、文档），不提交编译产物（见 `.gitignore`）。
- 每个驱动独立一个目录，目录内包含驱动 `.c`、测试 APP `.c` 和 `Makefile`。

## License

仅用于个人学习记录。
