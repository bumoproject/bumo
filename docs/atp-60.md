简介

ATP 60 (Asset Tokenization Protocol) 指基于BuChain的账本结构对商户的商品进⾏ SKU Token  发行、增发、转移、授权、兑付和纠纷，并支持认证证书和法律文件的标准协议。



## 目标

基于这套标准协议，可以使商户或企业更方便地将实体服务进行 Token 发行、流通、授权和兑付，以及纠纷处理。



## 属性描述规则

为了使属性能够像中心化数据库表一样方便存储、分层和查询，设计了json的双层嵌套列表结构（类似数据库表结构）。

### 原理

- 定义两层 json 嵌套列表，第二层 json列表的第一组列出各个列的意义。

- 第二层 json 列表从第二组开始就是用户数据。

- 不同 type 的数据，解析方式不同，这里会有一个词典。

  1. 第二层json列表的第一组的各个字段的意义

| 字段名    | 描述                                                         |
| --------- | ------------------------------------------------------------ |
| id        | 行号                                                         |
| parent_id | 父类行号，0指没有父类行号，其他值表示某行内容属于 parent_id 值所在行号。 |
| name      | 名称                                                         |
| type      | 数据类型                                                     |
| value     | 内容                                                         |
| decimals  | 浮点数的精度，其他类型为空                                   |
| uint      | 单位                                                         |

  2. 基本类型：

 **字符串: **

| 数据类型 | 描述                 |
| -------- | -------------------- |
| text     | value 可变长度字符串 |

 **数字类型：**

| 数据类型 | 描述                                                         |
| -------- | ------------------------------------------------------------ |
| int      | value 从 "-"2147483645 到 2147483647 的所有数字              |
| bigint   | value 从 "-"9223372036854775808 到 9223372036854775807 的所有数字 |
| float    | value 从 "-"1.79E + 308 到 1.79E + 308 的浮动精度数字数据    |

 **图片视频类型：**

| 数据类型 | 描述                               |
| -------- | ---------------------------------- |
| image    | value 表示为 [image格式\|url\|hash类型\|hash值] |
| mov    | value 表示为 [mov格式\|url\|hash类型\|hash值] |

 **文档类型：**

| 数据类型 | 描述                                           |
| -------- | ---------------------------------------------- |
| file     | value 表示为 [file格式\|url\|hash类型\|hash值] |

 **父行类型：**

| 数据类型 | 描述                       |
| -------- | -------------------------- |
| class    | 该类型的行是其他行的父类。 |

 **枚举类型：** 

| 数据类型 | 描述                                            |
| -------- | ----------------------------------------------- |
| enum     | value 显示当前枚举的值，uint 显示所有枚举的值。 |



### 优势

能够将属性像数据库的表结构一样存储，减少存储压力，且属性层次分明。



### 示例

```json 
key: sku_[sku_id]
value: { // 方便查找父id的位置
    "id": ["parentid", "name", "type", "value", "decimals", "uint"],
    "1": ["0",         "颜色",  "text", "白色",  "-",    　　"-"],
    "2": ["0",         "内存",  "int",   64,   　"-",      　G],
    "3": ["0",         "供货商", "class",  "-",   　"-",       "-"],
    "4": ["3",         "cpu", 　"text",  "三星",  "-",      "-"]
}
```



详解：

- 颜色和内存是最顶层的属性描述。
- 供货商是父类。
- cpu是在供货商这个父亲下的属性描述。



## 存储结构

这里描述了所有对象在区块链上的存储结构。



### 商户信息

这里主要描述了注册者的信息，包括：

- 基本信息：必填信息，包括公司名称、公司联系方式。
- 认证证书信息：基于各个地域的差异，填写方式不同，因此这里是自定义填写，是基于属性描述规则进行填写的。

存储结构如下：

```json
key: seller
value: {
    "address": "[商家区块链账户地址]",
    "companyFullName": "公司名称全称",
    "companyShortName": "公司名称简称",
    "conpanyContact": "公司联系方式", // 邮箱或电话
    // 自定义公司认证证书
    "companyCertification": {
        "id": ["parentid", "name", "type", "value", "decimals", "uint"],
        "5":  ["0",     "企业组织机构代码", "text", "N5464**", 　 "-",      "-"],
        "6":  ["0",     "法人姓名", "text",       "某某", 　　　  "-",      "-"],
        "7":  ["0",     "法人身份证号", "text",    "1**",        "-",      "-"],
        "8":  ["0",     "营业执照照片", "image", "[image格式|url|hash类型|hash值]",　"-",  "-"],
        "9":  ["0",     "法人身份证正照片", "image", "[image格式|url|hash类型|hash值]","-",  "-"],
        "10":  ["0",     "法人身份证反照片", "image", "[image格式|url|hash类型|hash值]","-", "-"]
    }
}
```





### 法律或其他认证文件

> 最多允许设置7000个文件。

- 描述

  法律文件或其他认证文件信息。

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 document 和文档编号组成的。
  2. value：这里所有的信息都是必填信息，包括文档名称，文档链接，文档编码类型，文档编码内容，文档提供者和上传日期。

- 示例

```json
key: document_[docId] // 文档编号
value: {
    "name": "文件名称",
    "url": "https://BUMO.io/BUMO-Technology-White-Paper-cn",
    "hashType": "md5",
    "hash": "31be016368639ba1a7ae7b63247807a1",
    "provider": "buQXRbzyDaVpX3JT3Wd2gj2U2ZzVWZRpwcng", //　文档提供者
    "date": 1544595438978280 // 上传日期
}
```



### 文件总量页数

> 总页数：总共不能超过2000页。
>
> 每页文件数量：每页所存文件列表转移为JSON字符串后，长度不得超过256000。

- 描述

  SPU 的页数。

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 documents_pages 组成的。
  2. value：页数量。

- 示例

```json
key: documents_pages
value: {
  "value": "1", // document 总数
  "pages": "1" // 页敉
}
```



### 文件列表

> 所存的认证文件的列表转移为JSON字符串后，长度不得超过256000。

- 描述

  法律文件或其他认证文件列表。

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 documents组成的。
  2. value：文件列表。

- 示例

```json
key: documents // 文档编号
value: ["documentId1", "documentId2", ...]
```



### SPU

- 描述

  SPU (Standard Product Unit，标准化产品单元)是商品信息聚合的最小单位，是一组可复用、易检索的标准化信息的集合，该集合描述了一个产品的特性。通俗点讲，属性值、特性相同的商品 就可以称为一个 SPU。 

  例例如:品牌苹果+型号:5s 可以确定一个产品，即 SPU。 

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 attribute 和 SPU 编号组成的。
  2. 基本信息：必填信息，包括兑付时将商品转移到指定账户的地址，SPU 名称和分类。
  3. 特性信息：由于各个 SPU 的特性不同，因此这里是自定义填写，是基于属性描述规则进行填写的。

- 示例

```json
key: spu_[spuId]
value: {
    "name": "苹果 5s",
    "type": "手机",
    "choice": { // 筛选 SKU 的信息
      "skuId": ["颜色", "内存", "型号"], // 筛选属性
      "1215451522": ["3", "4", "5"], //  编号1215451522的SKU的属性对应的属性id.
      "1324213423": ["3", "4", "5"], //  编号1324213423的SKU的属性对应的属性id.
      "1230902934": ["3", "4", "5"]  //  编号1230902934的SKU的属性对应的属性id.
    },
    // 自定义特性
    "attributes": {
        "id": ["parentid", "name", "type", "value", "decimals", "uint"],
        "4":  ["0",     　　"品牌",  "text",     "苹果",  　　          "-",    　 "-"],
        "5":  ["0",     　　"型号",  "text",     "5s",  　　           "-",    　 "-"]
    }
}
```



### Tranche 属性及限制

- 描述

  分片的相关属性及限制（目前只支持有效期的限制）。

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 balance、SKU 编号和 tranche 编码组成。
  2. value：指定 SKU Token 指定条件的余额。

- 示例

```json
key: tranche_[trancheId]
value: {
    "description": "有效期",
    "limits": {
        "validityPeriod": {
          "startTime": "1517032155872949", 
          "endTime":"1517470155872949"
        },
        ......
    }
}
```



### SKU 信息

- 描述

  SKU = Stock Keeping Unit (库存量单位)，即库存进出计量的单位，可以是以件，盒，托盘等为 单位。SKU 这是对于大型连锁超市 DC(配送中心)物流管理的一个必要的方法。当下已经被我 们引申为产品统一编号的简称，每种产品均对应有唯一的SKU号。 

  例如:品牌苹果+型号:5s可以确定一个产品，即 SPU。再加上颜色白色，尺码4.0，即表示一个 SKU。SPU + 颜⾊色 + 尺码，就是一个 SKU，SKU 是从属于 SPU 的关系 

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 sku_token 和 SKU 编号组成的。
  2. value中的基本信息：必填信息，包括 SPU 编码，Token 名称，Token 符号，Token 总量，Token 精度和Token 描述。
  3. value中的特性信息：由于商品的各个 SKU 是不同的，因此这里是自定义填写，是基于属性描述规则进行填写的。

- 示例

```json
key: sku_token_[skuId]
value: {
    "spuId": "[SPU 编码]", // 若无指向 spu，该字段可不赋值
    "defaultTrancheId": "[默认Tranche的id]", // 默认 tranche. 未设置，表示没有默认 tranche.
    "name": "[SKU 名称]",
    "symbol": "[Token 符号]",
    "faceValue": "$10", // 面值
    "totalSupply": "[Token 总量]",
    "decimals": "[Token 精度]",
    "mainIcon": "[image格式|url|hash类型|hash]", // 长度不得超过 10240
    "viceIcons": ["[image格式|url1|hash类型|hash]", "[image格式|url2|hash类型|hash]"], // 最多 5 个，且每个长度不得超过 10240
    "labels": ["[标签1]", "[标签2]", ...],  // 最多 20 个，且每个标签长度不得超过1024
    "description": "[SKU 描述]",
    "redemptionAddress": "[兑付区块链账户地址]", // 兑付成功时，商家将商品的 Token 转移到该地址
    //"controller": ,
    "abstract": ["1","3", "5"], // 最多 20 个，且每个摘要属性的长度不得超过 64
    "attributes": {
       "id": ["parentid", "name", "type", "value", "decimals", "uint"],
       "1":  ["0",     　 "参考价","int",       3000,  　　           "-",    　"CNY"],
       "2":  ["0",     　　"icon", "image",     "[image格式|url|hash类型|hash值]", "-",    　 "-"],
       "3":  ["0",     　　"颜色", "text",     "白色",                  "-",    　"-"],
       "4":  ["0",     　 "内存", "int",        64,                  "-",    　 "G"],
       "5":  ["0",     　 "型号", "text",     "中国大陆",             "-",    　 "-"]
    },
    "authorizers": ["buxxxxxxxxxxxxx", "buyyyyyyyyyyyyyy", ...] // 最多 10 个
    "time": "[发行时间戮]"
}
```



### SKU 指定 Tranche 的发行总量

- 描述

  指定 SKU 的指定 Tranche 的发行总量。

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 sku_tranche_supply，SKU 编号 和 Tranche 编号组成的。
  2. value：发行总量。

- 示例

```json
key: sku_tranche_supply_[skuId]_[trancheId]
value: "1000"
```



### SKU 的 Tranche 总量页数

> 总页数：总共不能超过2000页。
>
> 每页 Tranche 数量：每页所存 Tranche 的列表转移为JSON字符串后，长度不得超过256000。

- 描述

  指定 SKU 的 Tranche 的页数。

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 sku_tranches_pages 和 SKU 编号 组成的。
  2. value：页数量。

- 示例

```json
key: sku_tranches_pages_[skuId]
value: {
  "value": "10000", // tranche 总数
  "pages": "1" // 页敉
}
```



### SKU 的 Tranche 列表

> 所存 Tranche 的列表转移为JSON字符串后，长度不得超过256000。

- 描述

  Tranche 列表。

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 sku_tranches_page ，SKU 编号 和 页下标 组成的。
  2. value：Tranche 列表。

- 示例

```json
key: sku_tranches_page_[skuId]_[index]
value: ["trancheId1", "trancheId2", ...]
```





### SKU 的 Acceptance 总量页数

> 总页数：总共不能超过2000页。
>
> 每页 Acceptance 数量：每页所存 Acceptance 的列表转移为JSON字符串后，长度不得超过256000。

- 描述

  指定 SKU 的 Acceptance 的页数。

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 sku_acceptances_pages 和 SKU 编号 组成的。
  2. value：页数量。

- 示例

```json
key: sku_acceptances_pages_[skuId]
value: {
  "value": "10000", // acceptance 总数
  "pages": "1" // 页敉
}
```



### SKU 的 Acceptances 列表

> 所存 acceptances 的列表转移为JSON字符串后，长度不得超过256000。

- 描述

  Acceptances 列表。

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 sku_acceptances_page ，SKU 编号 和 页下标 组成的。
  2. value：Acceptance 列表。

- 示例

```json
key: sku_acceptances_page_[skuId]_[index]
value: ["acceptanceId1", "acceptanceId2", ...]
```



### Tranche 的 SKU 总量页数

> 总页数：总共不能超过2000页。
>
> 每页 SKU 数量：每页所存 SKU 的列表转移为JSON字符串后，长度不得超过256000。

- 描述

  指定 Tranche 的 SKU 的页数。

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 tranche_skus_pages 和 Tranche 编号 组成的。
  2. value：页数量。

- 示例

```json
key: tranche_skus_pages_[trancheId]
value: {
  "value": "10000", // sku 总数
  "pages": "1" // 页敉
}
```





### Tranche 的 SKU 列表

> 所存 SKU 的列表转移为JSON字符串后，长度不得超过256000。

- 描述

  SKU 列表。

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 tranche_skus_page ，Tranche 编号 和 页下标 组成的。
  2. value：SKU 列表。

- 示例

```json
key: tranche_skus_page_[trancheId]_[index]
value: ["skuId1", "skuId2", ...]
```



### 账户指定 SKU 余额

- 描述

  这里是方便查询账户指定SKU的库存。

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 balance、SKU 编号和账户地址组成。
  2. value：指定 SKU Token 的余额和 SKU 分类列表。

- 示例

```json
key: balance_[skuId]_[address]
value: "1000"
```



### 账户所有 Tranche 的Page数量

- 描述

  这里是方便查询账户的 Tranche，对 Tranche 进行分页，每页最多 2000 个 Tranche。最多支持 80  个 Page.

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 tranche_page、SKU 编号和账户地址组成。
  2. value：page的数值。

- 示例

```json
key: tranche_page_[skuId]_[address]
value: {
  "value": "10000", // tranche 总数
  "pages": "1" // 页敉
}
```



### 账户所有的 Tranche

- 描述

  账户下所有的 Tranche id 列表。

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 balance、SKU 编号，账户地址和 index 组成。
  2. value：指定 SKU Token 指定条件的余额。

- 示例

```json
key: tranches_[skuId]_[address]_[index]
value: {
    "tranches": ["trancheId1", "trancheId2", ...]
}
```



### 账户指定 SKU 的 Tranche 的余额

- 描述

  商品指定分片的余额。当锁定到期且在发送 Token 时或者手动解锁时，将已解锁的 Token 添加到可使用余额中。

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 balance_tranche、SKU 编号、 tranche 编码和账户地址组成。
  2. value：指定 SKU Token 指定条件的余额。

- 示例

```json
key: balance_tranche_[skuId]_[trancheId]_[address]
value: "1000"
```



### 授权

- 描述

  供应商对发行的 Token 的有效性进行授权。 

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 sku_authorization，SKU 编号和授权者区块链账户地址组成的。
  2. value：授权数量。

- 示例

```json
key: sku_authorization_[skuId]_[trancheId]_[authorizer]
value: '10000'
```



### 承兑信息

- 描述

  商家提供兑付信息。

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 acceptance 和承兑方编码组成。
  2. value的基本信息：必填，包括 SKU 编码，承兑方区块链账户地址，承兑方名称，承兑方联系方式，承兑方区块链账户地址和承兑有效期限。
  3. value中的附加信息：因各个地域的差异，因此这里是自定义填写，是基于属性描述规则进行填写的。

- 示例

```json
key: acceptance_[承兑方编码]
value: {
  "fullName": "[承兑方名称全称]",
  "shortName": "[承兑方名称简称]",
  "contact": "[承兑方联系方式]",
  "publicKey": "[承兑方区块链公钥]",
  "peroid": "[承兑有效期限]",
  "addition": { //　附加信息
   　"id": ["parentid", "name", "type", "value", "decimals", "uint"],
     "1": ["0", "承兑方地址", "text", "[承兑方地址]", "-", "-"],
     "2": ["0", "承兑方邮编", "text", "[承兑方邮编]", "-", "-"]
  }
}
```



### 兑付信息

- 描述

  提供兑付信息。

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 redemption，兑付序列号和申请人区块链账户地址组成。
  2. value的基本信息：必填，包括 承兑方区块链地址，tranche_id，数量，结果和状态。
  3. value中的附加信息：因各个地域的差异，因此这里是自定义填写，是基于属性描述规则进行填写的。

- 示例

```json
key: redemption_[兑付序列号]_[申请人区块链账户地址]
value: {
  "acceptanceId": "1",
  "skuId": "[skuId]",
  "trancheId": "[trancheId]",
  "value": "[SKU数量]",
  "information": "[Token信息]", // 这里可配置 tokens 的 id
  "status": 0, // 0 表示申请兑付，1 表示已兑付，2 表示兑付成功，3 表示兑付失败
  "requestTime": "[申请时间]",
  "sendTime": "[发货时间]",
  "finishTime": "[结束时间]",
  "addition": { // 附加信息
    "id": ["parentid", "name", "type", "value", "decimals", "uint"],
    "1":  ["0",     　　"买家信息",  "class",  "-",  　　      "-",    　 "-"],
    "2":  ["1",     　　"买家姓名",  "text",  "[买家姓名]",  　　      "-",    　 "-"],
    "3":  ["0",     　　"承兑方信息",  "class",  "-",  　　      "-",    　 "-"],
    "4":  ["3",     　　"承兑方姓名",  "text",  "[承兑方姓名]",  　　      "-",    　 "-"]
  }
}
```



### 纠纷信息

- 描述

  指定兑付纠纷的所有信息。

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 dispute 和 兑付序列号 组成 。
  2. value的基本信息：必填，包括 SKU 的 id，tranche 的 id，申请人区块链账户地址，纠纷原因，controller的区块链账户地址，纠纷处理状态，时间戮。
  3. value中的其他信息：因各个地域的差异，因此这里是自定义填写，是基于属性描述规则进行填写的。

- 示例

```json
key: dispute_[兑付序列号]_[兑付申请人区块链账户地址]
value: {
  "applicant": "[纠纷申请人区块链账户地址]",
  "reason": "[纠纷原因]",
  "status": "[纠纷处理结果]", // 0表示尚未处理，1申请人成功，2表示商家成功
  "controller": "buQWFjexkB8jSWxiSeczmgyHMPV94u5QbzW6", // 纠纷处理人，仅难道其设置最终的纠纷状态。
  "time": "[时间戮]",
  "addition": { // 附加信息
    "id": ["parentid", "name", "type", "value", "decimals", "uint"],
    "1":  [0,     　　"申请人",　"class", "-",　"-",　 "-"],
    "2":  [1,     　　"申请人姓名",　"text", "[申请人姓名]",　"-",　 "-"],
    "3":  [1,     　　"申请人联系方式",　"text", "[申请人联系方式]",　"-",　 "-"]
  }
}
```



### 纠纷证据

- 描述

  证据信息。只有兑付相关者（商家，兑付申请人和承兑方）可以提供证据，其他人不可以。

- 存储结构

  存储结构信息包括：

  1. key：索引关键字，由 evidence，兑付序列号，申请人区块链账户地址 和 证据提供者的区块链账户地址 组成 。
  2. value的基本信息：必填，包括 描述。
  3. value中的处理单位的其他信息：因各个地域的差异，因此这里是自定义填写，是基于属性描述规则进行填写的。

- 示例

```json
key: evidence_[兑付序列号]_[申请人区块链账户地址]_[证据提供者的区块链账户地址]
value: {
  "description": "[描述信息]",
  "addition": { // 附加信息
    "id": ["parentid", "name", "type", "value", "decimals", "uint"],
    "1":  [0,     　　"证据上传人姓名",　"text", "[证据上传人姓名]",　"-",　 "-"],
    "2":  [0,     　　"证据",　      "class",    "-",　           "-",　 "-"],
    "3":  [2,     　　"证据1",　"image", "[image格式|url|hash类型|hash值]",　"-",　 "-"],
    "4":  [2,     　　"证据2",　"image", "[image格式|url|hash类型|hash值]",　"-",　 "-"],
  }
}
```



## 接口定义

### 注册登记

在创建合约的时候，需要上传登记者信息和相应的认证证书信息。此时会创建一个 id 是 0 的默认 Tranche。

- 入口函数

  init

- 参数

  ```json
  {
    "companyFullName": "BUMO社区",
    "companyShortName": "BUMO",
    "companyContact": "Contact@bumo.io",
    "companyCertification": {
        "id": ["parentid", "name", "type", "value", "decimals", "uint"],
        "1":  ["0", "企业组织机构代码", "text", "N5464**", "-", "-"],
        "2":  ["0", "法人姓名", "text",     "某某",   "-",      "-"],
        "3":  ["0", "法人身份证号", "text",  "1**",   "-",      "-"],
        "4":  ["0", "营业执照照片", "image", "[image格式|url|hash类型|hash值]","-", "-"],
        "5":  ["0", "法人身份证正照片", "image", "[image格式|url|hash类型|hash值]","-", "-"],
        "6": ["0", "法人身份证反照片", "image", "[image格式|url|hash类型|hash值]","-", "-"]
    }
  }
  
  companyFullName：公司名称全称，长度[1, 1024]。
  companyShortName：公司名称简称，长度[1, 64]。
  companyContact：公司联系方式，长度[1, 64]。
  companyCertification：公司证件，可扩展字段。可选。
  ```

- 返回值

  成功：无返回值

  失败：抛异常



### 查询合约信息

该接口返回合约信息。

- 入口函数

  `query`

- 参数

  ```json
  {
      "method": "contractInfo"
  }
  ```

- 返回值

  成功：

  ```json
  {
    "name": "ATP60",
    "version": "1.0"
  }
  ```

  失败：抛异常



### 修改商家信息

修改商家信息。

- 入口函数

  `main`

- 参数

  ```json
  {
    "method": "setSeller",
    "params": {
      "companyFullName": "bumo",
      "companyShortName": "bumo",
      "companyContact": "Contact@bumo.io",
      "companyCertification": {
          "id": ["parentid", "name", "type", "value", "decimals", "uint"],
          "1":  ["0", "企业组织机构代码", "text", "N5464**", "-", "-"],
          "2":  ["0", "法人姓名", "text",     "某某",   "-",      "-"],
          "3":  ["0", "法人身份证号", "text",  "1**",   "-",      "-"],
          "4":  ["0", "营业执照照片", "image", "[image格式|url|hash类型|hash值]","-", "-"],
          "5":  ["0", "法人身份证正照片", "image", "[image格式|url|hash类型|hash值]","-", "-"],
          "6": ["0", "法人身份证反照片", "image", "[image格式|url|hash类型|hash值]","-", "-"]
      }
    }
  }
  
  companyFullName：公司名称全称，长度[1, 1024]。
  companyShortName：公司名称简称，长度[1, 64]。
  companyContact：公司联系方式，长度[1, 64]。
  companyCertification：公司证件，可扩展字段，但不可为空。可选。
  ```
  
- 返回值

  成功：无返回值

  失败：抛异常





### 查询商户信息

该接口返回商户的信息和证书信息。

- 入口函数

  `query`

- 参数

  ```json
  {
      "method": "sellerInfo"
  }
  ```

- 返回值

  成功：

  ```json
  {
    "address": "buQkeKX5wfsZScjePaL3BPdxSmX1cLwc37Ve",
    "companyFullName": "bumo",
    "companyShortName": "bumo",
    "conpanyContact": "Contact@bumo.io",
    "companyCertification": {
      "id": ["parentid", "name", "type", "value", "decimals", "uint"],
      "5":  ["0",  "企业组织机构代码", "text", "N5464**", 　"-",      "-"],
      "6":  ["0",  "法人姓名", "text",       "某某", 　　  "-",      "-"],
      "7":  ["0",  "法人身份证号", "text",    "1**",    "-",      "-"],
      "8":  ["0",  "营业执照照片", "image", "[image格式|url|hash类型|hash值]","-",  "-"],
      "9":  ["0",  "法人身份证正照片", "image", "[image格式|url|hash类型|hash值]","-", "-"],
      "10": ["0", "法人身份证反照片", "image", "[image格式|url|hash类型|hash值]","-", "-"]
    }
  }
  ```
  
  失败：抛异常



### 上传文件

> 该接口仅允许商家调用

该接口用于上传法律或其他认证文件。

- 入口函数

  `main`

- 参数

  ```json
  {
    "method": "setDocument",
    "params": {
      "id": "1",
      "name": "BUMO白皮书",
      "url": "https://BUMO.io/BUMO-Technology-White-Paper-cn",
      "hashType": "md5",
      "hash": "31be016368639ba1a7ae7b63247807a1"
    }
  }
  
  id：文档编号。长度[1, 32]。
  name：文档名称。长度[1, 1024]。
  url：文档链接。长度[1, 10240]。
  hashType：哈希类型。长度[1, 64]。
  hash：哈希值。长度[1, 2048]。
  ```

- 返回值

  成功：无返回值

  失败：抛异常



### 查询文件

该接口返回指定的法律文件或认证文件信息。

- 入口函数

  `query`

- 参数

  ```json 
  {
    "method": "documentInfo",
    "params": {
      "documentId": "文件编号"
    }
  }
  ```

- 返回值

  正常：

  ```json 
  {
    "name": "文件名称",
    "url": "https://BUMO.io/BUMO"-"Technology"-"White"-"Paper"-"cn",
    "hashType": "md5",
    "hash": "31be016368639ba1a7ae7b63247807a1",
    "provider": "buQXRbzyDaVpX3JT3Wd2gj2U2ZzVWZRpwcng",
    "date": 1544595438978280
  }
  ```

  失败：抛异常



### 查询所有的文档列表

该接口返回指定的法律文件或认证文件的id列表。

- 入口函数

  `query`

- 参数

  ```json 
  {
    "method": "allDocuments"
  }
  ```

- 返回值

  ```json 
  ["1"]
  ```



### 创建 SPU

> 该接口仅允许商家调用

商家创建 SPU。

- 入口函数

  `main`

- 参数

  ```json
  {
    "method": "createSpu",
    "params": {
      "id": "000000001",
      "name": "苹果 5s",
      "type": "手机",
      "attributes": {
        "id": ["parentid", "name", "type", "value", "decimals", "uint"],
        "1":  ["0",          "参考价", "int", "3000",    "-",       "CNY"],
        "2":  ["0",          "品牌",  "text", "苹果",   "-",      "-"],
        "3":  ["0",          "型号",  "text", "5s",    "-",      "-"]
      }
    }
  }
  
  id：SPU 的编号，长度[1, 32]。
  name：SPU 名称，长度[1, 1024]。
  type：SPU 类别，长度[1, 64]。
  attributes：SPU 属性。可选。
  ```

- 返回值

  成功：无返回值

  失败：抛出异常



### 修改 SPU

> 该接口仅允许商家调用

商家修改 SPU。

- 入口函数

  `main`

- 参数

  ```json
  {
    "method": "setSpu",
    "params": {
      "spuId": "000000001",
      "name": "苹果 5s",
      "type": "手机",
      "attributes": {
        "id": ["parentid", "name", "type", "value", "decimals", "uint"],
        "1":  ["0",          "参考价", "int", "3000",    "-",       "CNY"],
        "2":  ["0",          "品牌",  "text", "苹果",   "-",      "-"],
        "3":  ["0",          "型号",  "text", "5s",    "-",      "-"]
      }
    }
  }
  
  spuId：SPU 的编号，长度[1, 32]。
  name：SPU 名称，长度[1, 1024]。
  type：SPU 类别，长度[1, 64]。
  attributes：SPU 属性。可选。
  ```

- 返回值

  成功：无返回值

  失败：抛出异常





### 查询 SPU 信息

该接口返回指定 SPU 的所有信息。

- 入口函数

  `query`

- 参数

  ```json
  {
    "method": "spuInfo",
    "params": {
      "spuId": "000000001"
    }
  }
  ```

- 返回值

  成功：

  ```json
  {
      "name": "苹果 5s",
      "type": "手机",
      "attributes": {
        "id": ["parentid", "name", "type", "value", "decimals", "uint"],
        "2":  [0,          "参考价", "int", "3000",    "-",       "CNY"],
        "3":  [0,          "品牌",  "text", "苹果",   "-",      "-"],
        "4":  [0,          "型号",  "text", "5s",    "-",      "-"]
      }
  }
  ```
  

失败：抛异常



### 创建 Tranche

> 该接口仅允许商家调用

商家创建分片。用于将 SKU Token进行各种不同限制的分类，各个 tranche 之间不能相互交换。

- 入口函数

  `main`

- 参数

  ```json
  {
    "method": "createTranche",
    "params": {
      "id": "1",
      "description": "有效期",
      "limits": {
        "validityPeriod": {
          "startTime": "1517032155872949", // 开始时间，单位微秒
          "endTime":"1517470155872949" // 失效时间，单位微秒
        }
      }
    }
  }
  
  id: Tranche 编号，长度[1, 32]。
  description：Tranche 描述，长度[0, 64K]。
  limits：Tranche 约束，可为空。可选。
  ```

- 返回值

  成功：无返回值

  失败：抛出异常



### 查询 Tranche

查询 tranche 信息

- 入口函数

  `query`

- 参数

  ```json
  {
    "method": "trancheInfo",
    "params": {
      "trancheId": "1"
    }
  }
  ```

- 返回值

  ```json 
  {
    "description": "有效期",
    "limits": {
      "validityPeriod": {
        "startTime": "1517032155872949", // 开始时间，单位微秒
        "endTime":"1517470155872949" // 失效时间，单位微秒
      }
    }
  }
  ```





### 设置承兑信息

> 该接口仅允许商家调用。

商家指定承兑方信息，方便买家兑付商品。

- 入口函数

  `main`

- 参数

  ```json
  {
      "method": "setAcceptance",
      "params": {
          "id": "335695556",
          "publicKey": "b001223ec2424dece185915dde81e0c367365c9572dbf8d86e57e6f78766752ece349794b9fe",
          "fullName": "北京贸易集中处理中心",
          "shortName": "MC",
          "logo": "png|url|hashType|hash",
          "contact": "contact@my.com",
          "period": "7", //　单位: 天，承兑期限 
          "addition": {
              "id": ["parentid", "name", "type", "value", "decimals", "uint"],
              "1": ["0", "承兑方地址", "text", "[承兑方地址]", "-", "-"],
              "3": ["0", "承兑方邮编", "text", "[承兑方邮编]", "-", "-"]
          }
      }
  }
  
  id：兑付编号。长度[1, 32]。
  publicKey：承兑方区块链公钥。公钥对应的地址必须有效且账户在链上存在。在申请兑付时，兑付申请人可使用该公钥对私密信息进行加密处理。
  fullName：承兑方名称全称。长度[1, 1024]。
  shortName: 承兑方名称简称。长度[1, 64]。
  logo：承兑方logo。可选，字符串，长度[1, 10240]。
  contract：承兑方联系方式。长度[1, 64]。
  period：承兑期限。长度[1, 16]。
  addition：附加信息。可为空。可选。
  ```

- 返回值

  正常：无返回值

  失败：抛出异常



### 查询承兑信息

- 功能

  返回指定的承兑信息。

- 入口函数

  `query`

- 参数json结构

  ```json
  {
      "method":"acceptanceInfo",
      "params":{
          "acceptanceId":"335695556"
      }
  }
  ```

- 返回值

  ```json
  {
      "publicKey": "b001223ec2424dece185915dde81e0c367365c9572dbf8d86e57e6f78766752ece349794b9fe", // 承兑方区块链公钥
      "fullName": "北京贸易集中处理中心",
      "shortName": "MC",
      "logo": "png|url|hashType|hash",
      "contact": "contact@my.com",
      "period": "7", // 承兑期限，单位是天
      "addition": { //　附加信息
          "id": ["parentid", "name", "type", "value", "decimals", "uint"],
          "1": ["0", "承兑方地址", "text", "[承兑方地址]", "-", "-"],
          "3": ["0", "承兑方邮编", "text", "[承兑方邮编]", "-", "-"]
      }
  }
  ```





### 发行 SKU Token

> 1. 该接口仅允许商家调用
>
> 2. 默认 Tranche 不能进行转移，不能进行兑付。普通 Tranche 可进行转移和兑付。

该接口用于发行 SKU Token 到 Tranche。

- 入口函数

  main

- 参数

  ```json
  {
    "method": "issue",
    "params": {
      "skuId": "3333333215",
      "trancheId": "1",
      "isDefaultTranche": false,
      "spuId": "000000001",
      "name": "iphone 5s 白色 64G 中国大陆版",
      "symbol": "IPWSFC",
      "faceValue": "$10",
      "supply": "10000",
      "decimals": "0",
      "description": "iphone 5s 白色 64G 中国大陆版",
      "mainIcon": "[png|https://timgsa.baidu.com/timg?image&quality=80&size=b9999_10000&sec=1557751338635&di=3f6b989903ddf1cf9c10cc530c849d93&imgtype=0&src=http%3A%2F%2Fimg.zcool.cn%2Fcommunity%2F01815657c24f1b0000012e7eb901a1.jpg|md5|2938472190312847]",
      "viceIcons": ["png|https://timgsa.baidu.com/timg?image&quality=80&size=b9999_10000&sec=1557751338635&di=3f6b989903ddf1cf9c10cc530c849d93&imgtype=0&src=http%3A%2F%2Fimg.zcool.cn%2Fcommunity%2F01815657c24f1b0000012e7eb901a1.jpg|md5|2938472190312847"],
      "labels": ["iphone", "iphone 5s", "iphone 5s 白色", "iphone 5s 白色 64G", "iphone 5s 白色 64G 中国大陆版"],
      "redemptionAddress": "buQfVYf9Jvz9CJV9s2vzQaRkhMkfwn3BGFb1",
      "acceptanceId": "1",
      "abstract": ["2", "3", "4"],
      "attributes": {
        "id": ["parentid", "name", "type", "value", "decimals", "uint"],
        "1":  ["0",         "参考价", "int",  "3000",     "-",       "CNY"],
        "2":  ["0",         "颜色",  "text",  "白色",   "-",        "-"],
        "3": ["0",         "内存",  "int",   "64",       "-",        "G"],
        "4": ["0",         "型号",  "text",  "中国大陆", "-",       "-"]
      }
    }
  }
  
  skuId：SKU 的编号，长度 [1, 32]。
  trancheId：发行到的 tranche 的编号，长度[0, 32]。不设置，则使用 id 为 "0" 的 tranche。可选。
  isDefaultTranche：是否设置默认 trahche。true 表示设置默认 tranche，此时上方的 trancheId 即是默认 tranche，但是默认 tranche 不能进行转移和兑付。不设置或设置为false, 表示不配置，此时上方的 trancheId 是普通的 tranche，可进行转移和兑付。可选。
  spuId：SPU 的编号，若没有 SPU，可不赋值。可选。
  name：Token 名称。
  symbol：Token 符号。
  faceValue：面值，前面符号表示单位，后面是数值。可选。
  supply：Token 当前发行量。
  decimals：Token 精度。
  description：SKU 描述。可选。
  mainIcon：主图，长度[1, 10240]。可选。
  viceIcons：副图列表，最多5个，每个长度[1, 10240]。可选。
  redemptionAddress：回购区块链账户地址，当兑付完成时，将 SKU Token 转移到该地址。地址必须有效且账户在链上存在。
  acceptanceId：承兑方的编号，长度[1, 32]。
  abstract：摘要属性，表示用来描述SKU的必要属性。列表中填写的的attributes中的id。最多20个。可选。
  attributes：属性信息。可选。
  ```
  
- 返回值

  成功：无返回值

  失败：抛出异常



### 设置 SPU 下 SKU 的选项

> 该接口仅允许商家调用。

该接口用于设置 SPU 下 SKU 的选项。

- 入口函数

  main

- 参数

  ```json
  {
    "method": "setSkusChoice",
    "params": {
      "spuId": "1",
      "choice": {
        "skuId":      ["颜色", "内存", "型号"],
        "3333333215": [2,      3,      4],
        "3333333216": [2,      3,      4]
      }
    }
  }
  
  spuId：SPU 的编号，长度 [1, 32]。
  choice：根据 skuId 对应的数组里的元素进行 SKU 筛选。3333333215 和 3333333216　分别是两个 SKU 的编号， 2、3和4是对应 SKU 的 attributes里的属性编号。
  ```

- 返回值

  成功：无返回值

  失败：抛出异常



### 修改 SKU Token

> 该接口仅允许商家调用。

该接口用于修改 SKU 的信息，包括: 名称，Token 符号，主图，副图，标签，描述，回购账户地址，摘要属性列表，属性。

- 入口函数

  main

- 参数

  ```json
  {
    "method": "setSku",
    "params": {
      "skuId": "3333333215",
      "name": "iphone 5s 白色 64G 中国大陆版",
      "symbol": "IPWSFC",
      "description": "iphone 5s 白色 64G 中国大陆版",
      "mainIcon": "[png|https://timgsa.baidu.com/timg?image&quality=80&size=b9999_10000&sec=1557751338635&di=3f6b989903ddf1cf9c10cc530c849d93&imgtype=0&src=http%3A%2F%2Fimg.zcool.cn%2Fcommunity%2F01815657c24f1b0000012e7eb901a1.jpg|md5|2938472190312847]",
      "viceIcons": ["png|https://timgsa.baidu.com/timg?image&quality=80&size=b9999_10000&sec=1557751338635&di=3f6b989903ddf1cf9c10cc530c849d93&imgtype=0&src=http%3A%2F%2Fimg.zcool.cn%2Fcommunity%2F01815657c24f1b0000012e7eb901a1.jpg|md5|2938472190312847"],
      "labels": ["iphone", "iphone 5s", "iphone 5s 白色", "iphone 5s 白色 64G", "iphone 5s 白色 64G 中国大陆版"],
      "redemptionAddress": "buQfVYf9Jvz9CJV9s2vzQaRkhMkfwn3BGFb1",
      "abstract": ["2", "3", "4"],
      "attributes": {
        "id": ["parentid", "name", "type", "value", "decimals", "uint"],
        "1":  ["0",         "参考价", "int",  "3000",     "-",       "CNY"],
        "2":  ["0",         "颜色",  "text",  "白色",   "-",        "-"],
        "3": ["0",         "内存",  "int",   "64",       "-",        "G"],
        "4": ["0",         "型号",  "text",  "中国大陆", "-",       "-"]
      }
    }
  }
  
  skuId：SKU 的编号，长度 [1, 32]。
  name：Token 名称。不设置，则不修改。可选。
  symbol：Token 符号。不设置，则不修改。可选。
  description：SKU 描述。不设置，则不修改。可选。
  mainIcon：主图，长度[1, 10240]。不设置，则不修改。可选。
  viceIcons：副图列表，最多5个，每个长度[1, 10240]。不设置，则不修改。可选。
  redemptionAddress：回购区块链账户地址，当兑付完成时，将 SKU Token 转移到该地址。地址必须有效且账户在链上存在。不设置，则不修改。可选。
  abstract：摘要属性，表示用来描述SKU的必要属性。列表中填写的的attributes中的id。最多20个。不设置，则不修改。可选。
  attributes：属性信息。不设置，则不修改。可选。
  ```

- 返回值

  成功：无返回值

  失败：抛出异常



### 添加承兑方到 SKU

> 该接口仅允许商家调用。

该接口用于添加 Acceptance 的编号到 SKU。

- 入口函数

  main

- 参数

  ```json
  {
    "method": "addAcceptanceToSku",
    "params": {
      "skuId": "3333333215",
      "acceptanceId": "335695557"
    }
  }
  
  skuId：SKU 的编号，长度 [1, 32]。
  acceptanceId：承兑方的编号，长度[1, 32]。
  ```

- 返回值

  成功：无返回值

  失败：抛出异常



### 删除 SKU 承兑方

> 该接口仅允许商家调用。

该接口用于从 SKU 中删除指定的承兑方编号。

- 入口函数

  main

- 参数

  ```json
  {
    "method": "delAcceptanceFromSku",
    "params": {
      "skuId": "3333333215",
      "acceptanceId": "335695557"
    }
  }
  
  skuId：SKU 的编号，长度 [1, 32]。
  acceptanceId：承兑方的编号，长度[1, 32]。
  ```

- 返回值

  成功：无返回值

  失败：抛出异常



### 增发 SKU Token

> 1. 该接口仅允许商家调用。
> 2. 有默认 tranche 的 SKU 只能增发到 默认 tranche，不能增发到其他 tranche.

该接口用于增发 SKU Token 到指定 Tranche。

- 入口函数

  main

- 参数

  ```json
  {
    "method": "additionalIssuance",
    "params": {
      "skuId": "3333333215",
      "trancheId": "1",
      "supply": "10000"
    }
  }
  
  skuId：SKU 编号，长度[1, 32]。
  trancheId：Tranche 编号，长度[0, 32]。不设置，则使用 id 为 "0" 的 tranche。
  supply：当前增发数量。必须是数字字符串，且大于0，不得大于 int64 的最大值。
  ```
  
- 返回值

  成功：无返回值

  失败：抛出异常



### 分配到指定 Tranche

> 1. 该接口仅允许商家调用
> 2. 仅允许将含有默认 Tranche 的 SKU 进行分配，没有默认 Tranche 的 SKU 不能分配。

该接口用于将默认 Tranche 的 SKU Token 分配到指定 Tranche。

- 入口函数

  main

- 参数

  ```json
  {
    "method": "assignToTranche",
    "params": {
      "skuId": "3333333215",
      "toTrancheId": "1",
      "value": "1000"
    }
  }
  
  skuId：SKU 编号，长度[1, 32]。
  toTrancheId：目标 Tranche 编号，长度[0, 32]。不设置，则使用 id 为 "0" 的 tranche。可选。
  value：分配的数量。必须是数字字符串，且大于0，不得大于 int64 的最大值。
  ```
  
- 返回值

  成功：无返回值

  失败：抛出异常



### 设置 SKU 授权者

该接口用于设置授权者列表。

- 入口函数

  `main`

- 参数

  ```json
  {
    "method": "setAuthorizers",
    "params": {
      "skuId": "3333333215",
      "authorizers": ["buQYrKjnMioZwNam53nq85u4kJoKpzcdHx4x"]
    }
  }
  
  skuId: SKU 编号。
  authorizers: 授权者地址列表。不可为空。
  ```

- 返回值

  成功：无返回值

  失败：抛异常





### SKU 授权

该接口用于厂商或品牌商对商家发行的 SKU Token 的授权。

- 入口函数

  `main`

- 参数

  ```json
  {
    "method": "authorizeSku",
    "params": {
      "skuId": "3333333215",
      "trancheId": "1"
    }
  }
  
  skuId: SKU 编号。
  trancheId: tranche 编号，长度[0, 32]。不设置，则使用 id 为 "0" 的 tranche。可选。
  ```

- 返回值

  成功：无返回值

  失败：抛异常



### 查询 SKU 授权数量

该接口用于查询 SKU 被授权发行的数量。

- 入口函数

  query

- 参数

  ```json
  {
    "method": "authorizedSku",
    "params": {
      "skuId": "3333333215",
      "trancheId": "1",
      "authorizer": "buQsJDCmebe4hVYPxrY6KYCbx6K7dAapRqA3"
    }
  }
  
  skuIds: SKU 编号列表。
  ```

- 返回值

  成功：

  ```json
  '10000'
  ```

  失败：抛异常





### 查询 SPU 下的 所有 SKU

该接口返回指定 SPU 的所有 SKU id列表。

- 入口函数

  `query`

- 参数

  ```json
  {
    "method": "skusOfSpu",
    "params": {
      "spuId": "1"
    }
  }
  ```

- 返回值

  成功：

  ```json
  {
    "skuIds": ["3333333215"]
  }
  ```

  失败：抛异常



### 查询 Tranche 下的 所有 SKU

该接口返回指定 SPU 的所有 SKU id列表。

- 入口函数

  `query`

- 参数

  ```json
  {
    "method": "skusOfTranche",
    "params": {
      "trancheId": "1"
    }
  }
  ```

- 返回值

  成功：

  ```json
  ["3333333215"]
  ```

  失败：抛异常



### 查询 SKU 所有 Tranche

该接口用于返回指定 SKU 所有 Tranche 的 id 列表。

- 入口函数

  main

- 参数

  ```json
  {
    "method": "tranchesOfSku",
    "params": {
      "skuId": "3333333215"
    }
  }
  
  skuId：SKU 编号，长度[1, 32]。
  ```
  
- 返回值

  ```json 
  ["1", "2"]
  ```





### 查询账户所有 Tranche

该接口返回所有 Tranche id 列表。

- 入口函数

  `query`

- 参数

  ```json
  {
    "method": "tranchesOf",
    "params": {
      "skuId": "3333333215",
      "address": "buQj1rWK1XRXhCQLxfnR4CN3NFFjBAscyJu3"
    }
  }
  ```

- 返回值

  成功：

  ```json
  ["1", "2"]
  ```
  
  失败：抛异常



### 查询 SKU 所有承兑方

该接口用于返回指定 SKU 所有承兑方的 id 列表。

- 入口函数

  main

- 参数

  ```json
  {
    "method": "acceptancesOfSku",
    "params": {
      "skuId": "3333333215"
    }
  }
  
  skuId：SKU 编号，长度[1, 32]。
  ```

- 返回值

  ```json 
  ["1", "2"]
  ```



### 查询 SKU 信息

该接口返回指定 SKU Token 的所有信息。

- 入口函数

  `query`

- 参数

  ```json
  {
    "method": "tokenInfo",
    "params": {
      "skuId": "3333333216"
    }
  }
  ```

- 返回值

  成功：

  ``` json
  {
      "defaultTrancheId": "1", // 不赋值或赋值0，指id为0的tranche
      "spuId": "000000001", // SPU编号，若没有，可不赋值
      "name": "iphone 5s 白色 64G 中国大陆版",
      "symbol": "IPWSFC",
      "faceValue": "$10", // 面值
      "totalSupply": "10000",
      "decimals": "0",
      "description": "iphone 5s 白色 64G 中国大陆版",
      "mainIcon": "[png|https://timgsa.baidu.com/timg?image&quality=80&size=b9999_10000&sec=1557751338635&di=3f6b989903ddf1cf9c10cc530c849d93&imgtype=0&src=http%3A%2F%2Fimg.zcool.cn%2Fcommunity%2F01815657c24f1b0000012e7eb901a1.jpg|md5|2938472190312847]",
      "viceIcons": ["png|https://timgsa.baidu.com/timg?image&quality=80&size=b9999_10000&sec=1557751338635&di=3f6b989903ddf1cf9c10cc530c849d93&imgtype=0&src=http%3A%2F%2Fimg.zcool.cn%2Fcommunity%2F01815657c24f1b0000012e7eb901a1.jpg|md5|2938472190312847"],
      "labels": ["iphone", "iphone 5s", "iphone 5s 白色", "iphone 5s 白色 64G", "iphone 5s 白色 64G 中国大陆版"],
      "redemptionAddress": "buQfVYf9Jvz9CJV9s2vzQaRkhMkfwn3BGFb1",
      "attributes": { // 自定义特性
        "id": ["parentid", "name", "type", "value", "decimals", "uint"],
        "1":  ["0",         "参考价", "int",  "3000",     "-",       "CNY"],
        "2":  ["0",         "颜色",  "text",  "白色",   "-",        "-"],
        "3": ["0",         "内存",  "int",   "64",       "-",        "G"],
        "4": ["0",         "型号",  "text",  "中国大陆", "-",       "-"]
      },
    }
  ```
  
  失败：抛异常




### 查询账户SKU Token余额

查询指定账户的 SKU Token 余额。

- 入口函数

  `query`

- 参数

  ```json
  {
    "method": "balanceOf",
    "params": {
      "skuId": "3333333215",
      "address": "buQYH2VeL87svMuj2TdhgmoH9wSmcqrfBner"
    }
  }
  ```

- 返回值

  ```json 
  "1000"
  ```



### 查询账户指定 Tranche 的余额

查询指定账户指定 Tranche 的 SKU Token 余额

- 入口函数

  `query`

- 参数

  ```json
  {
    "method": "balanceOfByTranche",
    "params": {
      "skuId": "3333333215",
      "trancheId": "1",
      "address": "buQYH2VeL87svMuj2TdhgmoH9wSmcqrfBner"
    }
  }
  ```

- 返回值

  ```json 
  "1000"
  ```



### 销毁指定 Tranche 的 SKU Token

> 1. 该接口仅允许商家调用。
>2. 商家只有销毁自己的 token。

该接口用于销毁指定 tranche 的 SKU Token。

- 入口函数

  `main`

- 参数

  ```json
  {
    "method": "destroy",
    "params": {
      "address": "buQYH2VeL87svMuj2TdhgmoH9wSmcqrfBner",
      "skuId": "3333333215",
      "trancheId": "1",
      "value": "1000"
    }
  }

  address：待销毁的账户地址。地址必须有效且账户在链上存在。
  skuId：SKU 编号。长度[1, 32]。
  trancheId：Tranche 编号。长度[0, 32]。不设置，则使用 id 为 "0" 的 tranche。可选。
  value：待销毁的数量。必须是数字字符串，且大于0，不得大于 int64 的最大值。
  ```



- 返回值

  成功：无返回值

  失败：抛出异常





### 授权转账

- 功能

  授权账户 spender 可以从交易发送者账户默认 tranche 转出数量为 value 的 SKU token。

- 入口函数 

  `main`

- 参数json结构

  ```json
  {
      "method":"approve",
      "params":{
          "spender":"buQnTmK9iBFHyG2oLce7vcejPQ1g5xLVycsj",
          "skuId": "3333333217",
          "trancheId": "1",
          "value":"1000000"
      }
  }
  
  spender：被授权的区块链账户地址。地址必须有效且账户在链上存在。
  skuId：SKU 编号。长度[1, 32]。
  trancheId：Tranche 编号。长度[0, 32]。不设置，表示使用 id 为 0 的 tranche。可选。
  value：待授权的数量。必须是数字字符串，且大于0，不得大于 int64 的最大值。
  ```

- 返回值

  true 或者抛异常



### 查询授权数量

- 功能

  返回 spender 仍然被允许从 owner 指定 tranche 提取的指定 SKU token 金额。

- 入口函数

  `query`

- 参数json结构

  ```json
  {
      "method":"allowance",
      "params":{
          "owner":"buQnTmK9iBFHyG2oLce7vcejPQ1g5xLVycsj",
          "spender":"buQYH2VeL87svMuj2TdhgmoH9wSmcqrfBner",
          "skuId": "3333333217",
          "trancheId": "1"
      }
  }
  ```

- 返回值

  ```json
  "1000000"
  ```



### SKU Token 从指定地址转移

该接口实现将默认 Tranche 的 SKU Token 从指定地址的指定 tranche 转移到另一个地址的 tranche。

- 入口函数

  `main`

- 参数

  ```json
  {
      "method": "transferFrom",
      "params": {
          "from": "buQYH2VeL87svMuj2TdhgmoH9wSmcqrfBner",
          "skuId": "3333333217",
          "trancheId": "1",
          "to": "buQoP2eRymAcUm3uvWgQ8RnjtrSnXBXfAzsV",
          "value": "200"
      }
  }
  
  from：源账户地址。地址必须有效且账户在链上存在。
  skuId：SKU 编号。长度[1, 32]。
  trancheId：Tranche 编号。长度[0, 32]。不设置，表示使用 id 为 0 的 tranche。可选。
  to：目标账户地址。地址必须有效且账户在链上存在。
  value：待转移的数量。必须是数字字符串，且大于0，不得大于 int64 的最大值。
  ```

- 返回值

  成功：无返回值

  失败：抛异常



### SKU Token 转移

该接口用于将指定 Tranche 的 SKU Token 转移到另一个地址。

- 入口函数

  `main`

- 参数

  ```json
  {
      "method": "transfer",
      "params": {
          "skuId": "3333333217",
          "trancheId": "1",
          "to": "buQYH2VeL87svMuj2TdhgmoH9wSmcqrfBner",
          "value": "200"
      }
  }
  
  skuId：SKU 编号。长度[1, 32]。
  trancheId：Tranche 编号。长度[0, 32]。不设置，表示使用 id 为 0 的 tranche。可选。
  to：目标账户地址。地址必须有效且账户在链上存在。
  value：待转移的数量。必须是数字字符串，且大于0，不得大于 int64 的最大值。
  ```

- 返回值

  成功：无返回值

  失败：抛异常



### 申请兑付

> 1. 仅限 token 持有人调用该接口。
> 2. 当addition中有买家私密信息的时候，可通过使用承兑的公钥对信息进行加密处理。 

该接口用于 token 持有人申请将 token 换为实物。

- 入口函数

  `main`

- 参数

  ```json
  {
      "method": "requestRedemption",
      "params": {
          "redemptionId": "13241234234",
          "skuId": "3333333215",
          "trancheId": "1",
          "value": "100",
          "acceptanceId": "1",
          "description": "要好的商品",
          "addition": {
              "id": ["parentid", "name", "type", "value", "decimals", "uint"],
              "1":  ["0",        "买家信息", "class", "-",    "-",     "-"],
              "2":  ["1",        "买家姓名", "text", "[买家姓名]", "-",  "-"],
              "3":  ["0",        "承兑方信息", "class", "-",   "-",     "-"],
              "4":  ["3",        "承兑方姓名", "text", "[承兑方姓名]", "-", "-"]
          }
      }
  }
  
  redemptionId：兑付编号。长度[1, 32]。
  skuId：SKU 编号。长度[1, 32]。
  trancheId：Tranche 编号。长度[0, 32]。不设置，表示使用 id 为 0 的 tranche。可选。
  value：兑付数量。必须是数字字符串，且大于0，不得大于 int64 的最大值。
  acceptanceId：承兑方编号。长度[1, 32］。
  addition：附加信息。可为空。可选。
  ```
  
- 返回值

  正常：无返回值

  失败：抛出异常



### 兑付

> 1. 该接口仅允许承兑方调用。
>2. 在纠纷处理过程中，不允许调用该接口。

承兑方将 token 对应的实物发给 token 持有人。

- 入口函数

  `main`

- 参数

  ```json
  {
    "method": "redeem",
    "params": {
      "redemptionId": "13241234234",
      "applicant": "buQj1rWK1XRXhCQLxfnR4CN3NFFjBAscyJu3"
    }
  }
  
  redemptionId：兑付编号。长度[1, 32]。
  applicant：兑付申请人区块链账户地址。地址必须有效且账户在链上存在。
  ```
  
- 返回值

  正常：无返回值

  失败：抛出异常



### 确认兑付

> 1. 在承兑期间，仅允许兑付申请人调用。承兑超时，任何人都可以调用该接口。
>2. 在纠纷处理过程中，不允许调用该接口。

兑付申请人确认收到实物。

- 入口函数

  `main`

- 参数

  ```json
  {
      "method": "confirmRedemption",
      "params": {
          "redemptionId": "13241234234",
          "applicant": "buQj1rWK1XRXhCQLxfnR4CN3NFFjBAscyJu3"
      }
  }
  ```

- 返回值

  正常：无返回值

  失败：抛出异常



### 查询兑付信息

- 功能

  返回指定的兑付信息。

- 入口函数

  `query`

- 参数json结构

  ```json
  {
      "method":"redemptionInfo",
      "params":{
          "redemptionId":"13241234234",
          "applicant": "buQj1rWK1XRXhCQLxfnR4CN3NFFjBAscyJu3"
      }
  }
  ```

- 返回值

  ```json
  {
    "skuId": "3333333215",
    "trancheId": "1",
    "value": "100",
    "acceptanceId": "1", // 承兑方编号
    "status": 0, // 0 表示申请兑付，1 表示已兑付，2 表示兑付成功，3 表示兑付失败
    "requestTime": "1231321654654",
    "sendTime": "1326646456511",
    "finishTime": "145651321654654",
    "addition": { // 附加信息
      "id": ["parentid", "name", "type", "value", "decimals", "uint"],
      "1":  ["0",        "买家信息", "class", "-",    "-",     "-"],
      "2":  ["1",        "买家姓名", "text", "[买家姓名]", "-",  "-"],
      "3":  ["0",        "承兑方信息", "class", "-",   "-",     "-"],
      "4":  ["3",        "承兑方姓名", "text", "[承兑方姓名]", "-", "-"]
    }
  }
  ```



### 发起纠纷申请

> 该接口仅允许商家和兑付申请人调用。

商家或兑付申请人根据在兑付中遇到的问题发起纠纷申请。

- 入口函数

  `main`

- 参数

  ```json
  {
    "method": "applyDispute",
    "params": {
      "redemptionId":"13241234234",
      "applicant": "buQj1rWK1XRXhCQLxfnR4CN3NFFjBAscyJu3",
      "reason": "未付款",
      "controller": "buQWFjexkB8jSWxiSeczmgyHMPV94u5QbzW6",
      "addition": {
        "id": ["parent_id", "name", "type", "value", "decimals", "uint"],
        "1":  ["0",         "申请人", "class", "-", "-", "-"],
        "2":  ["1",         "申请人姓名", "text", "[申请人姓名]", "-", "-"],
        "3":  ["1",         "申请人联系方式", "text", "[申请人联系方式]", "-", "-"]
      }
    }
  }
  
  redemptionId：兑付编号。长度[1, 32]。
  applicant：兑付申请人区块链账户地址。地址必须有效且账户在链上存在。
  address：纠纷申请人区块链账户地址。地址必须有效且账户在链上存在。
  reason：纠纷原因。长度[1, 64K]。
  addition：附加信息。可选。
  ```
  
- 返回值

  正常：无返回值

  失败：抛出异常



### 提供证据

> 该接口仅允许商家，兑付申请人和承兑方调用。

商家，被兑付方和承兑方都可以提供证据，其他人不可以。

- 入口函数

  `main`

- 参数

  ```json
  {
    "method": "setEvidence",
    "params": {
      "redemptionId":"13241234234",
      "applicant": "buQj1rWK1XRXhCQLxfnR4CN3NFFjBAscyJu3",
      "description": "未付款",
      "addition": {
        "id": ["parent_id", "name", "type", "value", "decimals", "uint"],
        "1":  ["0",         "证据上传人姓名", "text",   "[证据上传人姓名]", "-", "-"],
        "2":  ["0",         "证据",  "class", "-",    "-", "-"],
        "3":  ["2",         "证据1", "image", "[image格式|url|hash类型|hash值]", "-", "-"],
        "4":  ["2",         "证据2", "image", "[image格式|url|hash类型|hash值]", "-", "-"]
      }
    }
  }
  
  redemptionId：兑付编号。长度[1, 32]。
  applicant：兑付申请人区块链账户地址。地址必须有效且账户在链上存在。
  description：描述。长度[1, 64K]。
  addition:附加信息。可选。
  ```

- 返回值

  正常：无返回值

  失败：抛出异常



### 查询证据

- 功能

  返回指定的信息。

- 入口函数

  `query`

- 参数json结构

  ```json
  {
      "method":"evidenceInfo",
      "params":{
          "redemptionId":"13241234234",
          "applicant": "buQnTmK9iBFHyG2oLce7vcejPQ1g5xLVycsj",
          "provider": "buQkeKX5wfsZScjePaL3BPdxSmX1cLwc37Ve"
      }
  }
  ```

- 返回值

  ```json
  {
      "description": "未付款",
      "addition": { // 附加信息
          "id": ["parentid", "name", "type", "value", "decimals", "uint"],
          "1":  ["0",     　　"证据上传人姓名",　"text", "[证据上传人姓名]",　"-",　 "-"],
          "2":  ["0",     　　"证据",　      "class",    "-",　           "-",　 "-"],
          "3":  ["2",     　　"证据1",　"image", "[image格式|url|hash类型|hash值]",　"-",　 "-"],
          "4":  ["2",     　　"证据2",　"image", "[image格式|url|hash类型|hash值]",　"-",　 "-"],
      }
  }
  ```



### 纠纷处理

> 该接口仅允许纠纷中的 controller 调用。

 Controller 根据证据进行纠纷处理。设置纠纷和兑付状态。

- 入口函数

  `main`

- 参数

  ```json
  {
      "method": "handleDispute",
      "params": {
          "redemptionId":"13241234234",
          "applicant": "buQnTmK9iBFHyG2oLce7vcejPQ1g5xLVycsj",
          "status": 1,
          "description": "描述信息"
      }
  }
  
  redemptionId：兑付编号。长度[1, 32]。
  applicant：兑付申请人区块链账户地址。地址必须有效且账户在链上存在。
  status：纠纷处理结果。1 或 2。1 表示兑付申请人获胜， 2表示商家获胜。
  description：描述，长度[1, 64K]。可选。
  ```

- 返回值

  正常：无返回值

  失败：抛出异常



### 查询纠纷信息

- 功能

  返回指定的兑付的纠纷信息。

- 入口函数

  `query`

- 参数json结构

  ```json
  {
      "method":"disputeInfo",
      "params":{
          "redemptionId":"13241234234",
          "applicant": "buQj1rWK1XRXhCQLxfnR4CN3NFFjBAscyJu3"
      }
  }
  ```

- 返回值

  ```json
  {
    "applicant": "buQj1rWK1XRXhCQLxfnR4CN3NFFjBAscyJu3",
    "reason": "未付款",
    "status": "0", // 0表示尚未处理，1申请人成功，2表示商家成功
    "controller": "buQWFjexkB8jSWxiSeczmgyHMPV94u5QbzW6", // 纠纷处理人，仅难道其设置最终的纠纷状态。
    "time": "1354665432134654",
    "addition": { // 附加信息
      "id": ["parentid", "name", "type", "value", "decimals", "uint"],
      "1":  [0,     　　"申请人",　"class", "-",　"-",　 "-"],
      "2":  [1,     　　"申请人姓名",　"text", "[申请人姓名]",　"-",　 "-"],
      "3":  [1,     　　"申请人联系方式",　"text", "[申请人联系方式]",　"-",　 "-"]
    }
  }
  ```



## 错误码

| 错误码 | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| 20000  | The companyFullName must be a string and its length must be between 1 and 1024. |
| 20001  | The companyShortName must be a string and its length must be between 1 and 64. |
| 20002  | The companyContact must be a string and its length must be between 1 and 64. |
| 20004  | The method type is invalid.                                  |
| 20005  | The document id must be string and its length must be between 1 and 32. |
| 20006  | The fullName must be string and its length must be between 1 and 1024. |
| 20007  | The shortName must be string and its length must be between 1 and 64. |
| 20008  | The document url must be string and its length must be between 1 and 10240. |
| 20009  | The hash_type must be string and its length must be between 1 and 64. |
| 20010  | The hash must be string and its length must be between 1 and 2048. |
| 20011  | The sender should be a seller.                               |
| 20012  | The pages of all documents cannot be bigger than 2000.       |
| 20013  | The specified document does not exist.                       |
| 20014  | The spu id must be string and its length must be between 1 and 32. |
| 20015  | The spu type must be string and its length must be between 1 and 64. |
| 20016  | The spu already exists.                                      |
| 20017  | The spu does not exist.                                      |
| 20018  | The tranche id must be string and its length must be between 1 and 32. |
| 20019  | The tranche already exists.                                  |
| 20020  | The tranche does not exist.                                  |
| 20021  | The tranche id must be default tranche.                      |
| 20022  | The tranche id cannot be default tranche.                    |
| 20023  | The sku id must be string and its length must be between 1 and 64. |
| 20024  | The isDefaultTranche must be boolean.                        |
| 20025  | The token symbol must be string and its length must be between 1 and 16. |
| 20026  | The token supply must be bigger than 0.                      |
| 20027  | The token decimals must be between 0 and 8.                  |
| 20028  | The main icon must be string, and its length must be between 1 and 10240. |
| 20029  | The vice icons must be array, its size must be between 0 and 5 and each length must be between 1 and 10240. |
| 20030  | The description must be string and its length must be between 0 and 64K. |
| 20031  | The labels must be array, its size must be between 0 and 20 and each length must be between 1 and 1024. |
| 20032  | The redemption address is invalid.                           |
| 20033  | The abstracts must be array, its size must be between 1 and 20 and each length must be between 1 and 64. |
| 20034  | The length of the acceptanceId must be between 1 and 64.     |
| 20035  | The sku already exists.                                      |
| 20036  | The name must be string and its length must be between 1 and 1024. |
| 20037  | The acceptance already exists in sku.                        |
| 20038  | The pages of all acceptances in sku cannot be bigger than 2000. |
| 20039  | The acceptance does not exist in sku.                        |
| 20040  | The sku does not exist.                                      |
| 20041  | Only default tranche can be assigned.                        |
| 20043  | The owner is invalid.                                        |
| 20044  | The value must be bigger than 0.                             |
| 20045  | The target tranche cannot be default tranche.                |
| 20046  | The spender does not have the allowance.                     |
| 20047  | The allowance is not enough.                                 |
| 20048  | The balance is not enough.                                   |
| 20049  | The authorizers must be array, and the size must be between 1 and 5. |
| 20050  | The authorizer does not exist in authorizers of the sku.     |
| 20051  | The tranche id is not in sku tranches.                       |
| 20052  | The choice must be an object and the length of the choice that is converted to string must be between 1 and 1024. |
| 20053  | The address is invalid.                                      |
| 20055  | The seller cannot destroy other\'s tokens.                   |
| 20055  | The spender is invalid.                                      |
| 20056  | The from is a invalid address.                               |
| 20057  | The to is a invalid address.                                 |
| 20058  | The logo must be string, and its length must be between 1 and 10240. |
| 20059  | The contact must be a string and its length must be between 1 and 64. |
| 20060  | The period must be bigger than 0.                            |
| 20061  | The acceptance does not exist.                               |
| 20062  | The redemption id must be string and its length must be between 1 and 32. |
| 20063  | The applicant is invalid.                                    |
| 20065  | The redemption does not exist.                               |
| 20066  | The redemption status must be 0.                             |
| 20067  | The sender is not acceptor.                                  |
| 20068  | The redemption already exists.                               |
| 20069  | The redemption is in dispute.                                |
| 20070  | The sender should be the applicant of the redemption.        |
| 20071  | The reason must be string and its length must be between 0 and 64K. |
| 20072  | The sender must be sellr or redemption applicant.            |
| 20073  | The dispute cannot be applied when the redemption status is not 0 and 1. |
| 20074  | The dispute already exists.                                  |
| 20075  | The provider must be applicant, seller or acceptor.          |
| 20076  | The dispute does not exist.                                  |
| 20077  | The provider is invalid.                                     |
| 20078  | The evidence does not exist.                                 |
| 20079  | The status must be 1 or 2.                                   |
| 20080  | The dispute has finished.                                    |
| 20081  | The sender is not the controller in dispute.                 |
| 20082  | The controller is invalid.                                   |
| 20083  | The public key of the acceptor is invalid.                   |

