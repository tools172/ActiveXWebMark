# Mermaid 线型测试

## 流程图

```mermaid
graph TD
  A[开始] -->|实线| B{判断}
  B -.->|虚线| C[执行]
  B ==>|粗线| D[结束]
```

## 时序图

```mermaid
sequenceDiagram
  Alice->>Bob: 实线消息
  Bob-->>Alice: 虚线消息
```
