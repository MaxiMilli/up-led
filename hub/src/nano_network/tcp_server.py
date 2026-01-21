import asyncio
from typing import Dict, Tuple, Callable
from ..nano_network.nano_manager import NanoManager

class TCPServer:
    def __init__(self, client_connected_handler: Callable):
        self.client_connected_handler = client_connected_handler
        self.nano_manager = NanoManager()

    async def handle_client(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        """Handle incoming TCP connections from Nanos"""
        try:
            # Get client info
            peer_name = writer.get_extra_info('peername')
            print(f"DEBUG: handle_client started - New connection from {peer_name}")

            print(f"Neue Verbindung von {peer_name}")
            
            while True:  # Keep connection alive
                # Get data
                print("DEBUG: Waiting for data from client...")
                data = await reader.read(1024)

                if not data:
                    print(f"Connection closed by client {peer_name}")
                    break
                    
                # Process message
                message = data.decode().strip()
                # print(f"DEBUG: Raw message received: '{message}'")
                
                # Handle initial registration
                if message.startswith("register--"):
                    mac_address = message.split("--")[-1]
                    ip = peer_name[0]  # Get IP from connection info
                    
                    if mac_address:
                        print(f"Registering Nano with MAC: {mac_address}, IP: {ip}")
                        await self.nano_manager.register_nano(mac_address, ip)
                        await self.client_connected_handler(mac_address, ip, reader, writer)
                    else:
                        print(f"Invalid registration message from {peer_name}: {message}")
                        break
                
        except Exception as e:
            print(f"Error in client handling for {peer_name}: {str(e)}")
        finally:
            try:
                print(f"DEBUG: Closing connection for {peer_name}")
                writer.close()
                await writer.wait_closed()
            except:
                pass

    async def start_server(self, host: str, port: int):
        server = await asyncio.start_server(
            self.handle_client, 
            host, 
            port
        )
        print(f"TCP Server running on {host}:{port}")
        async with server:
            await server.serve_forever()
