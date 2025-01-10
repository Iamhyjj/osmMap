## 目录
- [地图寻路项目报告](#地图寻路项目报告)
	- [目录](#目录)
	- [1. 项目运行方法](#1-项目运行方法)
	- [2. 基本功能完成情况](#2-基本功能完成情况)
		- [1.利用tinyxml解析osm数据中解析地图节点及邻接表。](#1利用tinyxml解析osm数据中解析地图节点及邻接表)
		- [2.在线请求openstreetmap地图png作为前端显示](#2在线请求openstreetmap地图png作为前端显示)
		- [3.用户点击地图选取起点和终点，计算并显示两点之间的最短路径。](#3用户点击地图选取起点和终点计算并显示两点之间的最短路径)
	- [3. 额外功能完成情况](#3-额外功能完成情况)
		- [1. 支持不同寻路算法](#1-支持不同寻路算法)
		- [2. 支持任意选点的最近邻查询](#2-支持任意选点的最近邻查询)
		- [3. 支持途径点选择](#3-支持途径点选择)
	- [4. 实验中遇到的困难与解决方法](#4-实验中遇到的困难与解决方法)
		- [1. 无法找到路径问题](#1-无法找到路径问题)
		- [2. 初始算法效率较低](#2-初始算法效率较低)
			- [（1）为什么没有采用双向A\*算法](#1为什么没有采用双向a算法)
			- [（2）为什么没有采用斐波那契额堆](#2为什么没有采用斐波那契额堆)
			- [（3）为什么采用带权A\*算法](#3为什么采用带权a算法)
	- [5. 实验总结](#5-实验总结)


## 1. 项目运行方法
项目文件包括：
![image-2](https://github.com/user-attachments/assets/c0032daf-c776-4531-89e2-8e699105db98)
使用方法：
- 在终端当前目录下运行：
```
g++ tinyxml/tinyxml.cpp tinyxml/tinystr.cpp tinyxml/tinyxmlerror.cpp tinyxml/tinyxmlparser.cpp jsoncpp/json_reader.cpp jsoncpp/json_value.cpp jsoncpp/json_writer.cpp parse.cpp -o parse.exe
```
然后执行parse.exe文件，会生成map_data.json(压缩文件中已经给出，因此可以跳过这步)。
- 在终端当前目录下运行：
```
emcc quadTree.cpp osmMap.cpp tinyxml/tinyxml.cpp tinyxml/tinystr.cpp tinyxml/tinyxmlerror.cpp tinyxml/tinyxmlparser.cpp jsoncpp/json_reader.cpp jsoncpp/json_value.cpp jsoncpp/json_writer.cpp bindings.cpp -o frontend/exposed.js -lembind --preload-file map_data.json -s INITIAL_MEMORY=3221225472 -s ALLOW_MEMORY_GROWTH=1 -O3 -msimd128
```
编译代码，并为emscripten运行准备好文件（压缩文件中也已经给出，也可跳过）。
- 在frontend目录下运行：
```
python -m http.server
```
然后在浏览器输入：127.0.0.1:8000即可使用地图服务，点击initialize按钮后等待一分钟左右即可。建议不要点击多次initialize按钮，可能会引发意外的bug。   
**注意：由于地图前端请求openstreetmap网站，因此需要科学上网**

## 2. 基本功能完成情况

### 1.利用tinyxml解析osm数据中解析地图节点及邻接表。  
为略微节省初始化时间，项目先利用tinyxml，依据针对不同出行方式的不同道路tag，对驾驶、步行、骑行道路进行不同的邻接表构建邻接表，并使用json格式存储（这部分代码在parse.cpp中）。解析中考虑了道路的单行性（即oneway标签），因此构建的是有向图。  
同时，考虑到寻路结果的日常合理性以及加快计算速率两方面考虑，邻接表中针对不同道路类型附加了不同的权重（根据与高德等寻路软件的主流导航结果对比调试得到的参数），使得寻路时会更偏向更适合选择的出行方式的道路（例如驾驶时更倾向高速、主干道）。因此，构建的是边带权图。两点之间的边长使用haversine公式计算直线距离，并加以权重系数得到。   
权重设置利用unordered_map实现，以下是部分示例(以驾驶模式为例)：   
```
std::unordered_map<std::string, double> drivingWeightTable = {
	{"motorway", 0.5},
	{"trunk", 0.6},
	{"primary", 0.8},
	{"secondary", 1.0},
	{"tertiary", 1.2},
	{"unclassified", 1.5},
	{"residential", 1.8},
	{"motorway_link", 0.7},
	{"trunk_link", 0.8},
	{"primary_link", 0.9},
	{"secondary_link", 1.0},
	{"tertiary_link", 1.3},
	{"service",3.0},
	{"living_street",3.0}
};
```
邻接表中以节点的id作为键，可以快速查询其所有邻点id及距离。   
之后初始化时，读取预先解析出的json数据（map_data.json）即可，会节约一些时间。解析后的数据效果展示如下：  
```
"adjacencyLists" : 
{
	"cycling" : 
	{
		"10000864210" : 
		[
			{
				"distance" : 0.13780812579941978,
				"node" : "109739106"
			},
			{
				"distance" : 0.087422351596991785,
				"node" : "11524752437"
			}
		],
		"10000864213" : 
		[
			{
				"distance" : 0.062943679681194395,
				"node" : "109738728"
			}
		],
	......
	}
	"driving" : 
	{
		"10000864210" : 
		[
			{
				"distance" : 0.13780812579941978,
				"node" : "109739106"
			},
			{
				"distance" : 0.087422351596991785,
				"node" : "11524752437"
			}
		],
		......
	}
	"walking" : 
	{
		"10000864210" : 
		[
			{
				"distance" : 0.13780812579941978,
				"node" : "109739106"
			},
			{
				"distance" : 0.087422351596991785,
				"node" : "11524752437"
			}
		],
	......
	}
	"nodes" : 
	{
		"10000864210" : 
		{
			"lat" : 31.284654100000001,
			"lon" : 121.4607438
		},
		"10000864213" : 
		{
			"lat" : 31.2890126,
			"lon" : 121.4614534
		},
		......
	}
}
```

### 2.在线请求openstreetmap地图png作为前端显示
限制视角范围与项目osm数据范围一致，支持拖动、滚轮缩放。效果如下：  
![image-1](https://github.com/user-attachments/assets/08f00f94-88be-4911-a273-f36a9547cdff)


### 3.用户点击地图选取起点和终点，计算并显示两点之间的最短路径。
在未选取起点和重点的时候，用户可以点击地图来选取起点和终点。默认先点击的点是起点，之后是终点。一旦起点和终点都被选取之后，点击地图不再有效，需要拖动起点和终点图标来改变其位置，一旦起点或终点位置被改变，将重新计算路径并显示。此外，也可以点击reset按钮清除起点和终点，这样之后需要重新点击地图选择起点和终点。  
此外，用户还可以用上方的菜单选择不同的出行方式寻路。
总体效果如下：
![image-3](https://github.com/user-attachments/assets/ad036745-47c8-42b6-8a58-18cced348689)


## 3. 额外功能完成情况
### 1. 支持不同寻路算法
项目中用户可以通过菜单选择不同算法进行寻路，实现的算法有：A*算法和Dijkstra算法，均使用基于stl优先队列的堆优化实现，A\*算法的启发函数选择某点到终点的haversine函数（具体代码见osmMap.cpp中的osmMap类的AStarSearch和Dijkstra方法）。效果如下：
![image](https://github.com/user-attachments/assets/3a5db104-3885-4f89-8976-96f0434b97c2)
 
实现方法为：
```
Json::Value osmMap::findShortestPath(const std::string& startID, const std::string& endID ,const std::string& mode = "driving", const std::string& algorithm = "A*"){
    if(algorithm == "A*") return AStarSearch(startID,endID,mode);
    if(algorithm == "Dijkstra") return Dijkstra(startID,endID,mode);
}
```


### 2. 支持任意选点的最近邻查询
用户可以通过点击地图上的点或者使用搜索框搜索起点和终点。由于用户选择的点在地图上不一定存在，因此采用该点的最近点作为代替。这一点，使用了自己编写的四分树实现(quadTree.cpp)。四分树的主要方法：
```
public:
    QuadTree(){}
    QuadTree(double latMin, double lonMin, double latMax, double lonMax);
    void insert(Node* newNode);
    Node* queryNearest(double lat, double lon);
    void print();
```
初始化时，使用insert构建四叉树，用户选点时，使用queryNearest查询距离选择的坐标最近的节点。  
查询部分的代码：
```
Node* QuadTree::queryNearest(QuadTreeNode* node, double lat, double lon) {
    //leaf
    if(node->point!=nullptr){
        std::cout << "NearestNodeID : " << node -> point -> ID << '\n';
        return node->point;
    }
    
    Node* nearest = nullptr;
    bool notfound = 0;
    while(nearest == nullptr){
        // 如果是非叶节点，继续递归到对应的子树
        if (lat >= (node->latMin + node->latMax) / 2 || notfound) {
            if (lon >= (node->lonMin + node->lonMax) / 2 || notfound) {
                if( node -> NE && (node->NE->NE != nullptr || node->NE->point!=nullptr)){  //儿子为非空叶子节点或者非叶节点
                    nearest = queryNearest(node->NE, lat, lon); // 东北
                    notfound = 0;
                }
            } 
            if (lon < (node->lonMin + node->lonMax) / 2 || notfound) {
                if(node -> NW && (node->NW->NW != nullptr || node->NW->point!=nullptr)){
                    nearest = queryNearest(node->NW, lat, lon); // 西北
                    notfound = 0;
                }
            }
        }
        if (lat < (node->latMin + node->latMax) / 2 || notfound){
            if (lon >= (node->lonMin + node->lonMax) / 2 || notfound) {
                if(node -> SE && (node->SE->SE != nullptr || node->SE->point!=nullptr)){
                    nearest = queryNearest(node->SE, lat, lon); // 东南
                    notfound = 0;
                }
            } 
            if (lon < (node->lonMin + node->lonMax) / 2 || notfound) {
                if(node -> SW && (node->SW->SW != nullptr || node->SW->point!=nullptr)){
                    nearest = queryNearest(node->SW, lat, lon); // 西南
                    notfound = 0;
                }
            }
        }
        notfound = 1;
    }
    return nearest; 
}
```
查询时，根据给定经纬度查找所在范围，递归的到相应子树查找，直至叶子节点。若该叶子节点非空，则返回该点所存储的节点；否则寻找该叶子节点的非空兄弟节点，这时的查找不一定确保最近，但是由于抵达叶子时划分已经十分精细，兄弟节点之间的距离本身很小，因此不是最近的结果也可接受。
### 3. 支持途径点选择
用户可以通过按住ctrl+点击左键选择途径点，保证寻路结果一定经过途径点，途径点选择后可以通过拖动改变位置。实现方法为：调用两次后端寻路函数，一次从起点到途径点，一次从途径点到终点。前端实现代码：
```
function findAndDrawPath() {
	if (!startPoint?.nodeId || !endPoint?.nodeId) {
		console.error("Start or End node ID is missing.");
		return;
	}

	try {
		let result;
		// 无途径点
		if (!wayPointMarker) {
			result = Module.FindShortestWay(startPoint.nodeId.toString(), endPoint.nodeId.toString(), routeMode , algorithm);
		}
		// 有途径点
		else {
			// 获取途径点的 nodeId
			const waypointNodeId = fetchNearestNode(wayPointMarker.getLatLng().lat, wayPointMarker.getLatLng().lng);

			// 先计算从起点到途径点的路径
			const firstLegResult = Module.FindShortestWay(startPoint.nodeId.toString(), waypointNodeId.toString(), routeMode, algorithm);

			// 再计算从途径点到终点的路径
			const secondLegResult = Module.FindShortestWay(waypointNodeId.toString(), endPoint.nodeId.toString(), routeMode, algorithm);

			// 合并两段路径
			const firstLegPath = JSON.parse(firstLegResult);
			const secondLegPath = JSON.parse(secondLegResult);

			// 合并路径，去掉第二段路径的第一个点（避免途径点重复）
			const combinedPath = firstLegPath.concat(secondLegPath.slice(1));

			// 将合并后的路径保存到 result 中
			result = JSON.stringify(combinedPath);
		}
		const pathData = JSON.parse(result);
		if (Array.isArray(pathData) && pathData.length > 0) {
			if (currentPath) map.removeLayer(currentPath);
			currentPath = L.polyline(pathData, { color: 'blue' }).addTo(map);
			map.fitBounds(currentPath.getBounds());
			console.log("Path drawn on the map.");
		} else {
			alert("No valid path data returned.");
		}

	} catch (error) {
		console.error("Error finding shortest path:", error);
	}
}
```
效果如下：
![image-5](https://github.com/user-attachments/assets/62782a0b-9980-4653-81e4-392edaf2876d)

## 4. 实验中遇到的困难与解决方法
### 1. 无法找到路径问题
实验初期，在任意选择起点终点时，经常出现找不到路径的情况。经过调试发现，问题出在对道路的解析上。因此，我仔细学习了OpenStreetMap Wiki上的内容，对不同tag分类，为不同出行方式详细设置了tag表解析，使得不同出行方式可以正确寻路。特别的，在行人和骑行方式时，考虑到了上海渡江需要轮渡的特殊性，将其加入其中。具体的解析规则可以参照parse.cpp的parseWays函数实现。  
```
static const std::unordered_set<std::string> drivingHighways = {
"motorway", "trunk", "primary", "secondary", "tertiary",
"unclassified", "residential", "motorway_link", "trunk_link",
"secondary_link", "tertiary_link","primary_link","service","living_street"};

static const std::unordered_set<std::string> walkingHighways = {
	"footway", "path", "pedestrian", "steps", "cycleway","residential"
	,"living_street","secondary","corridor","track", "tertiary","service"};

static const std::unordered_set<std::string> cyclingHighways = {
	"cycleway", "primary", "secondary", "tertiary", "unclassified", "residential"
	,"track","primary_link","tertiary_link","secondary_link","service"};
......
if(key=="route"&&value=="ferry"){
	isWalkingAllowed = true;
	isCyclingAllowed =true;
}
```
### 2. 初始算法效率较低
实验中最开始使用A\*算法，但是经过计时，单次寻路最坏可达到几千毫秒。为改进这一点，考虑过三个方法：使用双向A\*算法、使用斐波那契额堆优化、使用带权A\*算法。最终只使用了最后一种优化方法，使得单次寻路最坏控制在几百毫秒内。  
#### （1）为什么没有采用双向A\*算法  
本次实验采用了邻接表存储有向图，双向A*会扩大一倍的邻接表存储空间，而邻接表本身已经很大，再扩大可能对运行时的内存造成较大负担。并且双向A\*算法无法带来数量级的优化，因此没有进行。  
#### （2）为什么没有采用斐波那契额堆  
较成熟的斐波那契堆（例如boost库）配置较为复杂，为项目开发带来困难。过程中尝试过使用较轻量级的斐波那契额堆(https://github.com/robinmessage/fibonacci.git) 但是经过测试其性能不如stl的优先队列, 因而放弃。

#### （3）为什么采用带权A\*算法  
如前所述，这是出于寻路结果的日常合理性以及加快计算速率两方面考虑，邻接表中针对不同道路类型附加了不同的权重（根据与高德等寻路软件的主流导航结果对比调试得到的参数），使得寻路时会非常偏向更适合选择的出行方式的道路（例如驾驶时更倾向高速、主干道），这使得算法不会耗时关注那些不合理且复杂的路径。  
这有可能在特殊情况下违背A\*算法要求的启发函数必须低估实际代价，但是绝大部分情况下，即使附加权重的距离计算，仍然是保守估计，即符合低估要求。在不符合低估要求的情况下，该节点距离终点已经非常近，可行的道路几乎只有一种可能，因此也不会错误寻路。  

经过测试，带权A*算法在正确性、合理性、效率上都远由于初始朴素A\*实现。一般情况下可以在几毫秒至几十毫秒内完成计算，在复杂道路下最坏需要几百毫秒完成。

## 5. 实验总结
本项目完成了实验基本要求，并在此基础上实现了基于四叉树的最近点寻找、不同算法的寻路、途径点选择的额外功能。同时，通过本次实验，我实际实现了了图算法（特别是A*算法和Dijkstra算法），熟悉了如何利用实际数据构建图模型，并在此基础上实现路径规划和导航功能。同时，我也提高了对前端开发的理解。
