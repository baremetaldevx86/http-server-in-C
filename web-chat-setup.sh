#!/bin/bash
# web_chat_setup.sh - Easy setup for web-based chat server

echo "🌐 Web Chat Server Setup"
echo "========================"

# Compile the web server
echo "📦 Compiling web chat server..."
gcc -o web_chat_server web_chat_server.c -lpthread

if [ $? -ne 0 ]; then
    echo "❌ Failed to compile server. Make sure you have gcc and pthread library."
    exit 1
fi

echo "✅ Compilation successful!"
echo ""

# Check firewall and suggest setup
echo "🛡️  Firewall Setup:"
echo "If others can't access the chat, allow port 8080:"

if command -v ufw >/dev/null 2>&1; then
    echo "  Run: sudo ufw allow 8080"
elif command -v firewall-cmd >/dev/null 2>&1; then
    echo "  Run: sudo firewall-cmd --permanent --add-port=8080/tcp && sudo firewall-cmd --reload"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    echo "  macOS: System Preferences → Security & Privacy → Firewall → Options"
else
    echo "  Check your firewall settings to allow port 8080"
fi

echo ""
echo "🚀 Starting web chat server..."
echo "   Access from any browser: http://[your-ip]:8080"
echo "   Press Ctrl+C to stop"
echo ""

# Start the server
./web-server
