## 工具

- Bun (or NPM, PNPM, YARN, etc)
- Go 1.22.4

## 编译

```shell
cd frontend
bun install
bun run build
cd ..
go build
```

## 使用说明

运行 `newclient`，打开 `http://localhost:8080`，指定 fleet leader 地址，点击 Set，选择测试用例和算法模式（暂未实现），点击 Run Test Case，查看结果。
