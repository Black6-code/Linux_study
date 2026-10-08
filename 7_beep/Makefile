KERNELDIR := /home/xyl/linux/IMX6ULL/linux/linux-imx-rel_imx_4.1.15_2.1.0_ga

CURRENT_PATH := $(shell pwd)

# NFS 根文件系统里的模块目录（部署目标）
NFS_DIR := /home/xyl/linux/nfs/rootfs/lib/modules/4.1.15

obj-m := beep.o

# 需要部署的模块(.ko)列表，自动由 obj-m 推导——以后增删模块只改 obj-m 一处
MODULES := $(obj-m:.o=.ko)

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean

# 一键更新 IntelliSense 编译数据库：
# 先 clean 确保 bear 能抓到全部编译命令，再编译并生成 compile_commands.json
ccdb:
	$(MAKE) clean
	bear -- $(MAKE) build

# 一键部署：先编译，再把 obj-m 里所有模块拷贝到 NFS 根文件系统
# 若目标目录已归你所有则无需 sudo；否则自动回退用 sudo
install: build
	@cp -f $(MODULES) $(NFS_DIR) 2>/dev/null || sudo cp -f $(MODULES) $(NFS_DIR)
	@echo ">>> 已部署: $(MODULES)"

.PHONY: build kernel_modules clean ccdb install
