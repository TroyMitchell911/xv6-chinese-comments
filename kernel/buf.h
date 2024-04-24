struct buf {
	// 是否包含该块的副本（是否从磁盘读取了数据）
  int valid;   // has data been read from disk?
  // 缓冲区的内容已经被修改需要被重新写入磁盘
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];
};

