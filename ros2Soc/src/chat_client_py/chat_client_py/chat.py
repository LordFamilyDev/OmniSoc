import threading
import rclpy
from rclpy.node import Node
from message_definitions.msg import ChatMessage
import sys

class ChatNode(Node):
    def __init__(self):
        super().__init__('chat_node')
        self.publisher_ = self.create_publisher(ChatMessage, 'chat', 10)
        self.subscription = self.create_subscription(ChatMessage, 'chat', self.listener_callback, 10)
        self.msg_count = 0

    def publish_message(self, message):
        self.msg_count += 1
        msg = ChatMessage()
        msg.number = self.msg_count
        msg.timestamp = self.get_clock().now().to_msg()
        msg.message = message
        self.publisher_.publish(msg)
        self.get_logger().info(f'Publishing: "{message}" with number {self.msg_count}')

    def listener_callback(self, msg):
        # Ensure the message is from another chat client
        if msg.number != self.msg_count:
            self.get_logger().info(f'I heard: [{msg.number}] "{msg.message}"')

def main(args=None):
    rclpy.init(args=args)
    chat_node = ChatNode()

    # Use a separate thread for spinning to handle callbacks
    executor = rclpy.executors.SingleThreadedExecutor()
    executor.add_node(chat_node)
    executor_thread = threading.Thread(target=executor.spin, daemon=True)
    executor_thread.start()

    try:
        while True:
            message = input("Enter your message (type 'exit' to quit): ")
            if message == 'exit':
                break
            chat_node.publish_message(message)
    except KeyboardInterrupt:
        pass
    finally:
        # Shutdown and cleanup
        rclpy.shutdown()
        executor.shutdown()

if __name__ == '__main__':
    main()
