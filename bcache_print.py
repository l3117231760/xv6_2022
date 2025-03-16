import gdb

class PrintBCacheCommand(gdb.Command):
    def __init__(self):
        super(PrintBCacheCommand, self).__init__("print_bcache", gdb.COMMAND_USER)
    
    def invoke(self, arg, from_tty):
        try:
            # 获取bcache全局变量
            bcache = gdb.parse_and_eval("bcache")
            # 获取链表头节点
            head = bcache['head']
            head_addr = head.address
            current = head['next']
            
            print("{:<8} {:<6} {:<8} {:<6} {:<6} {:<6}".format(
                "ADDR", "DEV", "BLOCK#", "REFCNT", "VALID", "DISK"))
            print("="*50)
            
            count = 0
            while current != head_addr:
                # 解引用缓冲区指针
                buf = current.dereference()
                # 读取关键字段
                addr = int(current)
                dev = int(buf['dev'])
                blockno = int(buf['blockno'])
                refcnt = int(buf['refcnt'])
                valid = int(buf['valid'])
                disk = int(buf['disk'])
                
                # 格式化输出
                print("0x{:<6x} {:<6} {:<8} {:<6} {:<6} {:<6}".format(
                    addr, dev, blockno, refcnt, valid, disk))
                
                # 移动到下一个节点
                current = buf['next']
                count += 1
            
            print("\nTotal {} buffers in bcache".format(count))
            
        except gdb.error as e:
            print("Error:", e)

PrintBCacheCommand()