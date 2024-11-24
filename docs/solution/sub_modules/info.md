# info

详细实现可参考: `convert/info`。

为了区分各种不同改写规则需要的信息，我们将每种改写规则对应的信息都定义在一个单独的类中，并都保存为 `Env` 的成员变量。

如下是一些常用的 `Info` 类:
- `CommonInfoLeaf`: 用于 `CommonInfo` 数据的改写，具体实现可参考: `convert/info/CommonInfoLeaf.h`。主要包含以下主要信息:
  - `common_info_value_`: `CommonInfo` 枚举对应的 `int` 值。
  - `list_loop_var_`: 遍历 `common_info` 列表时候用的变量。
  - `common_info_enum_name_`: `CommonInfo` 枚举名。 
- `ActionDetailInfo`: 用于 `ActionDetail` 数据的改写，具体实现可参考: `convert/info/ActionDetailInfo.h`。
- `AssignInfo`: `Assign` 语句的信息，具体实现可参考: `convert/info/AssignInfo.h`。

其他还有很多 `Info` 类，这里不再一一列举。