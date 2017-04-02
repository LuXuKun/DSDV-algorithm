---
title: DSDV算法
categories: 分布式系统
tags: Networking

---
## Introduction ##
本文章主要介绍DSDV算法的实现及模拟应用。DSDV是对传统的BF路由算法的一种修正，它着重解决了RIP面对断开的链接时发生的死循环现象，使之成为一种对于ad hoc网络更合适的路由协议。
## 1、初始化 ##
各节点从相应的input file中读取各邻居结点的距离，然后将信息(seq、destination、cost、nexthop等)存入自己的table（初始为空），将自己的信息也放入table中（destination为本节点，cost为0），将表中自己及其它节点对应的seq都设置为0。

## 2、定期广播 ##
各节点定期会把自己的table中的信息广播给自己的邻居，这里设置的时间间隔为10秒钟。

## 3、信息接收及更新 ##
当接收到来自邻居的table的信息时，对于邻居发来的table中的每一条entry（假设seq=s、destination=d、cost=c），如果自己的table中节点d对应的entry中的seq信息没有s大，那么己方table中的这条entry就应进行更新：
- 令seq=s，cost=c+己方结点与发来信息的邻居的距离，nexthop=d。
- 如果自己的table中节点d对应的entry中的seq信息与s相等，且cost>c+己方结点与发来信息的邻居的距离，那么己方table中的这条entry应更新cost=c+己方结点与发来信息的邻居的距离，nexthop=d。否则不做更改。
- 如果自己的table中节点d对应的entry中的seq信息大于s，则不做更改。

## 4、input file 更新 ##
- 各节点在每次广播信息之前，会先检查input file中的内容是否更改（即检查己方结点与邻居的直接距离是否改变），如果发生了更改（例如与邻居d的直接距离更改为了c），则将己方table中结点d对应的entry中的seq加一，cost改为c，nexthop改为d（虽然与邻居d直接相连，但之前的nexthop也可能不是d，而是另一条更小cost路径上的节点）。
- 同时，己方table中其他所有以d为nexthop的节点对应的entry也需要更新信息（因为现在己方结点与d的距离已更新），注意这些节点的seq也要加一。
- 最终d以及其它被己方节点增加过seq的节点均会收到自己新的seq，这些结点发现别人拥有的自己的seq比自己拥有的自己的seq还大时，就将自己的seq+2，然后将自己的信息广播出去，因为自己对自己的信息是最了解的，理应拥有自己最新的seq，这样可以解决以下类型的问题出现：
> 节点n1检测到input file中到邻居节点n2的距离发生了改变（假设变为-1，即无法再直接连接），则将自己table中n2对应的entry中的cost改为MAXCOST，seq加一，广播出去，这时接到信息的其它节点发现节点n1给出的到达节点n2的seq更高（因为已加一），则将自己的table中关于n2的seq更新为此更大的seq，并将到n2的路径更改为经由n1到n2，cost更改为了MAXCOST，但事实上可能有其它路径能到达n2，这时需要一个新的关于n2的广播到来，这个广播中关于n2的seq至少要大于等于n1给出的seq，才能使之前记录了错误信息的节点到达n2的路径再次更新，所以我们规定当有关n2的信息（seq=n1更新过的seq，cost=MAXCOST，destination=n2）到达n2本身时，n2会将自己的seq加二广播出去，以便其它节点正确更新到达自己的路径。

## 5、其他细节 ##
directcost与cost不同，directcost是指节点与邻居直接相连的代价，而cost指节点到达另一个结点所能找到的最小代价。
因为第3和第4步均可能有对table的更改操作，而两者的操作不在同一线程中，所以这里要加锁，以保证原子性操作。
