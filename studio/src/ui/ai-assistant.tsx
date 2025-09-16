/**
 * AI Assistant Component for KengaAI Studio
 * Интерфейс для работы с GigaChat AI помощником
 */

import React, { useState, useEffect, useRef } from 'react';
import { Button } from './button';
import { Textarea } from './textarea';
import { Card, CardHeader, CardTitle, CardContent } from './card';
import { Badge } from './badge';
import { Loader2, Send, Bot, User, Settings, RefreshCw, Trash2 } from 'lucide-react';
import GigaChatService, { ChatMessage, ProjectContext } from '../lib/ai/gigachat-service';

interface AIAssistantProps {
  projectContext?: ProjectContext;
  onCodeGenerated?: (code: string) => void;
  onSuggestionApplied?: (suggestion: string) => void;
}

export const AIAssistant: React.FC<AIAssistantProps> = ({
  projectContext,
  onCodeGenerated,
  onSuggestionApplied
}) => {
  const [isInitialized, setIsInitialized] = useState(false);
  const [isLoading, setIsLoading] = useState(false);
  const [messages, setMessages] = useState<ChatMessage[]>([]);
  const [currentMessage, setCurrentMessage] = useState('');
  const [service, setService] = useState<GigaChatService | null>(null);
  const [status, setStatus] = useState<'disconnected' | 'connecting' | 'connected' | 'error'>('disconnected');
  const messagesEndRef = useRef<HTMLDivElement>(null);

  // Инициализация AI сервиса
  useEffect(() => {
    initializeAI();
    return () => {
      if (service) {
        service.stop();
      }
    };
  }, []);

  // Автопрокрутка к последнему сообщению
  useEffect(() => {
    scrollToBottom();
  }, [messages]);

  const initializeAI = async () => {
    setStatus('connecting');
    try {
      const chatService = new GigaChatService({
        modelPath: 'models/GigaChat-20B-A3B-base',
        contextSize: 8192,
        temperature: 0.7,
        maxTokens: 2048,
        device: 'auto'
      });

      const success = await chatService.initialize();
      if (success) {
        setService(chatService);
        setIsInitialized(true);
        setStatus('connected');

        // Приветственное сообщение
        addMessage({
          role: 'assistant',
          content: `Привет! Я ваш AI-помощник для разработки на KengaAI Engine. 🤖

Я могу помочь вам с:
• 📝 Анализом и генерацией кода на Rust
• 🎮 Разработкой игровых механик
• 🛠️ Оптимизацией производительности
• 📚 Объяснением архитектуры движка
• 🐛 Поиском и исправлением ошибок

Что вы хотели бы сделать сегодня?`,
          timestamp: new Date()
        });
      } else {
        setStatus('error');
        addMessage({
          role: 'assistant',
          content: '❌ Не удалось инициализировать AI. Проверьте установку модели GigaChat.',
          timestamp: new Date()
        });
      }
    } catch (error) {
      setStatus('error');
      console.error('Ошибка инициализации AI:', error);
      addMessage({
        role: 'assistant',
        content: `❌ Ошибка инициализации: ${error instanceof Error ? error.message : 'Неизвестная ошибка'}`,
        timestamp: new Date()
      });
    }
  };

  const addMessage = (message: ChatMessage) => {
    setMessages(prev => [...prev, message]);
  };

  const scrollToBottom = () => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  };

  const sendMessage = async () => {
    if (!currentMessage.trim() || !service || !isInitialized) return;

    const userMessage: ChatMessage = {
      role: 'user',
      content: currentMessage.trim(),
      timestamp: new Date()
    };

    addMessage(userMessage);
    setCurrentMessage('');
    setIsLoading(true);

    try {
      const response = await service.generateResponse(currentMessage, projectContext);

      const assistantMessage: ChatMessage = {
        role: 'assistant',
        content: response,
        timestamp: new Date()
      };

      addMessage(assistantMessage);
    } catch (error) {
      console.error('Ошибка отправки сообщения:', error);
      addMessage({
        role: 'assistant',
        content: `❌ Ошибка: ${error instanceof Error ? error.message : 'Не удалось получить ответ'}`,
        timestamp: new Date()
      });
    } finally {
      setIsLoading(false);
    }
  };

  const handleKeyPress = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      sendMessage();
    }
  };

  const clearHistory = () => {
    setMessages([]);
    if (service) {
      service.clearHistory();
    }
  };

  const reconnect = () => {
    if (service) {
      service.stop();
    }
    setService(null);
    setIsInitialized(false);
    setStatus('disconnected');
    initializeAI();
  };

  const getStatusBadge = () => {
    switch (status) {
      case 'connected':
        return <Badge variant="default" className="bg-green-500">Подключен</Badge>;
      case 'connecting':
        return <Badge variant="secondary">Подключение...</Badge>;
      case 'error':
        return <Badge variant="destructive">Ошибка</Badge>;
      default:
        return <Badge variant="outline">Отключен</Badge>;
    }
  };

  const quickActions = [
    {
      label: 'Анализ кода',
      prompt: 'Проанализируй текущий файл и предложи улучшения'
    },
    {
      label: 'Генерация компонента',
      prompt: 'Создай новый игровой компонент для системы частиц'
    },
    {
      label: 'Оптимизация',
      prompt: 'Как оптимизировать рендеринг для мобильных устройств?'
    },
    {
      label: 'Документация',
      prompt: 'Объясни архитектуру системы освещения в KengaAI Engine'
    }
  ];

  return (
    <Card className="h-full flex flex-col">
      <CardHeader className="pb-3">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-2">
            <Bot className="h-5 w-5 text-blue-500" />
            <CardTitle className="text-lg">AI Помощник</CardTitle>
            {getStatusBadge()}
          </div>
          <div className="flex items-center gap-1">
            <Button
              variant="ghost"
              size="sm"
              onClick={reconnect}
              disabled={status === 'connecting'}
            >
              <RefreshCw className={`h-4 w-4 ${status === 'connecting' ? 'animate-spin' : ''}`} />
            </Button>
            <Button
              variant="ghost"
              size="sm"
              onClick={clearHistory}
              disabled={!isInitialized}
            >
              <Trash2 className="h-4 w-4" />
            </Button>
          </div>
        </div>
      </CardHeader>

      <CardContent className="flex-1 flex flex-col p-4 pt-0">
        {/* Сообщения */}
        <div className="flex-1 overflow-y-auto mb-4 space-y-3">
          {messages.map((message, index) => (
            <div
              key={index}
              className={`flex gap-3 ${
                message.role === 'user' ? 'justify-end' : 'justify-start'
              }`}
            >
              {message.role === 'assistant' && (
                <Bot className="h-6 w-6 text-blue-500 mt-1 flex-shrink-0" />
              )}
              <div
                className={`max-w-[80%] p-3 rounded-lg ${
                  message.role === 'user'
                    ? 'bg-blue-500 text-white'
                    : 'bg-gray-100 text-gray-900'
                }`}
              >
                <div className="whitespace-pre-wrap text-sm">
                  {message.content}
                </div>
                {message.timestamp && (
                  <div className="text-xs opacity-70 mt-2">
                    {message.timestamp.toLocaleTimeString()}
                  </div>
                )}
              </div>
              {message.role === 'user' && (
                <User className="h-6 w-6 text-gray-500 mt-1 flex-shrink-0" />
              )}
            </div>
          ))}
          <div ref={messagesEndRef} />
        </div>

        {/* Быстрые действия */}
        {messages.length === 0 && (
          <div className="mb-4">
            <h4 className="text-sm font-medium mb-2">Быстрые действия:</h4>
            <div className="grid grid-cols-2 gap-2">
              {quickActions.map((action, index) => (
                <Button
                  key={index}
                  variant="outline"
                  size="sm"
                  onClick={() => setCurrentMessage(action.prompt)}
                  disabled={!isInitialized}
                  className="text-xs h-auto py-2 px-3 text-left whitespace-normal"
                >
                  {action.label}
                </Button>
              ))}
            </div>
          </div>
        )}

        {/* Ввод сообщения */}
        <div className="flex gap-2">
          <Textarea
            value={currentMessage}
            onChange={(e) => setCurrentMessage(e.target.value)}
            onKeyPress={handleKeyPress}
            placeholder={
              isInitialized
                ? "Задайте вопрос об KengaAI Engine..."
                : "Подключение к AI..."
            }
            disabled={!isInitialized}
            className="min-h-[60px] resize-none"
            rows={2}
          />
          <Button
            onClick={sendMessage}
            disabled={!isInitialized || !currentMessage.trim() || isLoading}
            size="icon"
            className="self-end"
          >
            {isLoading ? (
              <Loader2 className="h-4 w-4 animate-spin" />
            ) : (
              <Send className="h-4 w-4" />
            )}
          </Button>
        </div>

        {/* Статус */}
        <div className="mt-2 text-xs text-gray-500">
          {status === 'connecting' && 'Инициализация модели...'}
          {status === 'connected' && 'AI готов к работе'}
          {status === 'error' && 'Ошибка подключения к AI'}
          {status === 'disconnected' && 'AI отключен'}
        </div>
      </CardContent>
    </Card>
  );
};

export default AIAssistant;
