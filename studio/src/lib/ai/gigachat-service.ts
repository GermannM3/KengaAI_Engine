/**
 * GigaChat AI Service for KengaAI Studio
 * Интеграция русской языковой модели для помощи в разработке
 */

import { spawn } from 'child_process';
import * as fs from 'fs';
import * as path from 'path';

export interface GigaChatConfig {
  modelPath: string;
  contextSize: number;
  temperature: number;
  maxTokens: number;
  device: 'cpu' | 'cuda' | 'auto';
}

export interface ChatMessage {
  role: 'user' | 'assistant' | 'system';
  content: string;
  timestamp?: Date;
}

export interface ProjectContext {
  files: string[];
  currentFile?: string;
  projectStructure: any;
  recentChanges: string[];
}

export class GigaChatService {
  private config: GigaChatConfig;
  private conversationHistory: ChatMessage[] = [];
  private isInitialized = false;
  private pythonProcess: any = null;

  constructor(config: Partial<GigaChatConfig> = {}) {
    this.config = {
      modelPath: config.modelPath || 'models/GigaChat-20B-A3B-base',
      contextSize: config.contextSize || 8192,
      temperature: config.temperature || 0.7,
      maxTokens: config.maxTokens || 2048,
      device: config.device || 'auto',
    };
  }

  /**
   * Инициализация модели GigaChat
   */
  async initialize(): Promise<boolean> {
    try {
      // Проверка наличия модели
      if (!fs.existsSync(this.config.modelPath)) {
        console.log('Модель не найдена, начинаем загрузку...');
        await this.downloadModel();
      }

      // Проверка зависимостей Python
      await this.checkPythonDependencies();

      // Запуск Python сервиса для модели
      await this.startPythonService();

      this.isInitialized = true;
      console.log('✅ GigaChat успешно инициализирован');
      return true;
    } catch (error) {
      console.error('❌ Ошибка инициализации GigaChat:', error);
      return false;
    }
  }

  /**
   * Загрузка модели из HuggingFace
   */
  private async downloadModel(): Promise<void> {
    return new Promise((resolve, reject) => {
      console.log('📥 Загрузка модели GigaChat-20B-A3B-base...');

      const pythonScript = `
import os
from huggingface_hub import snapshot_download

model_path = "${this.config.modelPath}"
os.makedirs(model_path, exist_ok=True)

print("Загрузка модели...")
snapshot_download(
    repo_id="ai-sage/GigaChat-20B-A3B-base",
    local_dir=model_path,
    local_dir_use_symlinks=False
)
print("✅ Модель загружена")
      `;

      const downloadProcess = spawn('python3', ['-c', pythonScript], {
        stdio: 'inherit',
        cwd: process.cwd()
      });

      downloadProcess.on('close', (code) => {
        if (code === 0) {
          resolve();
        } else {
          reject(new Error('Ошибка загрузки модели'));
        }
      });

      downloadProcess.on('error', reject);
    });
  }

  /**
   * Проверка Python зависимостей
   */
  private async checkPythonDependencies(): Promise<void> {
    const requiredPackages = [
      'torch',
      'transformers',
      'accelerate',
      'huggingface_hub',
      'sentencepiece'
    ];

    return new Promise((resolve, reject) => {
      const checkScript = `
import sys
import subprocess
import pkg_resources

required = ${JSON.stringify(requiredPackages)}
missing = []

for package in required:
    try:
        pkg_resources.get_distribution(package)
        print(f"✓ {package}")
    except pkg_resources.DistributionNotFound:
        missing.append(package)
        print(f"✗ {package}")

if missing:
    print(f"Установка недостающих пакетов: {', '.join(missing)}")
    subprocess.check_call([sys.executable, '-m', 'pip', 'install'] + missing)
    print("✅ Все зависимости установлены")
else:
    print("✅ Все зависимости установлены")
      `;

      const checkProcess = spawn('python3', ['-c', checkScript], {
        stdio: 'inherit'
      });

      checkProcess.on('close', (code) => {
        if (code === 0) {
          resolve();
        } else {
          reject(new Error('Ошибка проверки зависимостей'));
        }
      });
    });
  }

  /**
   * Запуск Python сервиса для работы с моделью
   */
  private async startPythonService(): Promise<void> {
    return new Promise((resolve, reject) => {
      const serviceScript = path.join(__dirname, 'gigachat_worker.py');

      // Создание Python worker скрипта
      const workerScript = `
import sys
import json
import torch
from transformers import AutoTokenizer, AutoModelForCausalLM, GenerationConfig
import traceback

class GigaChatWorker:
    def __init__(self):
        self.model = None
        self.tokenizer = None
        self.device = "${this.config.device}"
        if self.device == "auto":
            self.device = "cuda" if torch.cuda.is_available() else "cpu"

    def load_model(self, model_path):
        print(f"Загрузка модели с {self.device}...")
        self.tokenizer = AutoTokenizer.from_pretrained(model_path, trust_remote_code=True)

        # Оптимизации для памяти
        if self.device == "cuda":
            self.model = AutoModelForCausalLM.from_pretrained(
                model_path,
                torch_dtype=torch.bfloat16,
                device_map="auto",
                trust_remote_code=True,
                load_in_8bit=True  # 8-bit quantization
            )
        else:
            self.model = AutoModelForCausalLM.from_pretrained(
                model_path,
                torch_dtype=torch.float32,
                device_map={"": "cpu"},
                trust_remote_code=True,
                low_cpu_mem_usage=True
            )

        self.model.generation_config = GenerationConfig.from_pretrained(model_path)
        print("✅ Модель загружена")

    def generate(self, prompt, max_tokens=2048, temperature=0.7):
        try:
            inputs = self.tokenizer(prompt, return_tensors="pt").to(self.model.device)

            with torch.no_grad():
                outputs = self.model.generate(
                    **inputs,
                    max_new_tokens=max_tokens,
                    temperature=temperature,
                    do_sample=True,
                    pad_token_id=self.tokenizer.eos_token_id
                )

            response = self.tokenizer.decode(outputs[0][inputs.input_ids.shape[1]:], skip_special_tokens=True)
            return response.strip()
        except Exception as e:
            return f"Ошибка генерации: {str(e)}"

# Основной цикл обработки
if __name__ == "__main__":
    worker = GigaChatWorker()
    worker.load_model("${this.config.modelPath}")

    for line in sys.stdin:
        try:
            data = json.loads(line.strip())
            if data["action"] == "generate":
                response = worker.generate(
                    data["prompt"],
                    data.get("max_tokens", 2048),
                    data.get("temperature", 0.7)
                )
                result = {"response": response, "status": "success"}
            else:
                result = {"error": "Неизвестное действие", "status": "error"}
        except Exception as e:
            result = {"error": str(e), "status": "error"}

        print(json.dumps(result, ensure_ascii=False))
        sys.stdout.flush()
      `;

      fs.writeFileSync(serviceScript, workerScript);

      // Запуск Python процесса
      this.pythonProcess = spawn('python3', [serviceScript], {
        stdio: ['pipe', 'pipe', 'pipe'],
        cwd: process.cwd()
      });

      let initialized = false;

      this.pythonProcess.stdout.on('data', (data) => {
        const output = data.toString().trim();
        if (output.includes('✅ Модель загружена') && !initialized) {
          initialized = true;
          resolve();
        }
        console.log('Python:', output);
      });

      this.pythonProcess.stderr.on('data', (data) => {
        console.error('Python Error:', data.toString());
      });

      this.pythonProcess.on('close', (code) => {
        if (!initialized) {
          reject(new Error(`Python процесс завершился с кодом ${code}`));
        }
      });

      // Таймаут на инициализацию
      setTimeout(() => {
        if (!initialized) {
          reject(new Error('Таймаут инициализации модели'));
        }
      }, 300000); // 5 минут
    });
  }

  /**
   * Генерация ответа на основе контекста проекта
   */
  async generateResponse(message: string, context?: ProjectContext): Promise<string> {
    if (!this.isInitialized) {
      throw new Error('GigaChat не инициализирован');
    }

    // Формирование промпта с контекстом
    let prompt = this.buildPrompt(message, context);

    // Добавление в историю
    this.conversationHistory.push({
      role: 'user',
      content: message,
      timestamp: new Date()
    });

    try {
      const response = await this.callPythonService(prompt);

      // Добавление ответа в историю
      this.conversationHistory.push({
        role: 'assistant',
        content: response,
        timestamp: new Date()
      });

      return response;
    } catch (error) {
      console.error('Ошибка генерации:', error);
      return 'Извините, произошла ошибка при генерации ответа.';
    }
  }

  /**
   * Построение промпта с учетом контекста проекта
   */
  private buildPrompt(message: string, context?: ProjectContext): string {
    let systemPrompt = `Ты - AI-помощник для разработчиков игрового движка KengaAI Engine.
Ты знаешь Rust, WebGPU, 3D графику и разработку игр.
Отвечай на русском языке, будь полезным и конкретным.

`;

    if (context) {
      systemPrompt += `
КОНТЕКСТ ПРОЕКТА:
- Файлы проекта: ${context.files.join(', ')}
- Текущий файл: ${context.currentFile || 'не выбран'}
- Структура проекта: ${JSON.stringify(context.projectStructure, null, 2)}
- Последние изменения: ${context.recentChanges.join('; ')}
`;
    }

    // Добавление истории разговора (последние 5 сообщений)
    const recentHistory = this.conversationHistory.slice(-5);
    if (recentHistory.length > 0) {
      systemPrompt += '\nИСТОРИЯ РАЗГОВОРА:\n';
      recentHistory.forEach(msg => {
        systemPrompt += `${msg.role}: ${msg.content}\n`;
      });
    }

    return systemPrompt + `\nПОЛЬЗОВАТЕЛЬ: ${message}\n\nОТВЕТ:`;
  }

  /**
   * Вызов Python сервиса для генерации
   */
  private async callPythonService(prompt: string): Promise<string> {
    return new Promise((resolve, reject) => {
      const request = {
        action: 'generate',
        prompt: prompt,
        max_tokens: this.config.maxTokens,
        temperature: this.config.temperature
      };

      const requestJson = JSON.stringify(request) + '\n';

      let responseData = '';

      const timeout = setTimeout(() => {
        reject(new Error('Таймаут ожидания ответа'));
      }, 60000); // 1 минута

      const onData = (data: Buffer) => {
        responseData += data.toString();

        // Проверка на завершение JSON ответа
        if (responseData.includes('\n')) {
          try {
            const lines = responseData.trim().split('\n');
            const lastLine = lines[lines.length - 1];
            const result = JSON.parse(lastLine);

            clearTimeout(timeout);
            this.pythonProcess.stdout.off('data', onData);

            if (result.status === 'success') {
              resolve(result.response);
            } else {
              reject(new Error(result.error || 'Ошибка генерации'));
            }
          } catch (e) {
            // JSON еще не завершен, продолжаем сборку
          }
        }
      };

      this.pythonProcess.stdout.on('data', onData);
      this.pythonProcess.stdin.write(requestJson);
    });
  }

  /**
   * Анализ кода с помощью AI
   */
  async analyzeCode(code: string, language: string = 'rust'): Promise<{
    explanation: string;
    suggestions: string[];
    issues: string[];
  }> {
    const prompt = `Проанализируй следующий ${language} код и дай подробное объяснение:

\`\`\`${language}
${code}
\`\`\`

Ответь в формате JSON:
{
  "explanation": "подробное объяснение кода",
  "suggestions": ["предложение1", "предложение2"],
  "issues": ["проблема1", "проблема2"]
}`;

    const response = await this.generateResponse(prompt);

    try {
      return JSON.parse(response);
    } catch {
      return {
        explanation: response,
        suggestions: [],
        issues: []
      };
    }
  }

  /**
   * Генерация кода на основе описания
   */
  async generateCode(description: string, language: string = 'rust'): Promise<string> {
    const prompt = `Создай ${language} код для игрового движка KengaAI Engine на основе описания:

${description}

Код должен быть:
- Современным и идиоматичным
- Хорошо документированным
- Оптимизированным
- Соответствовать архитектуре KengaAI Engine

Ответь только кодом, без лишних комментариев:`;

    return await this.generateResponse(prompt);
  }

  /**
   * Очистка истории разговора
   */
  clearHistory(): void {
    this.conversationHistory = [];
  }

  /**
   * Остановка сервиса
   */
  async stop(): Promise<void> {
    if (this.pythonProcess) {
      this.pythonProcess.kill();
      this.pythonProcess = null;
    }
    this.isInitialized = false;
  }

  /**
   * Получение статуса сервиса
   */
  getStatus(): {
    initialized: boolean;
    config: GigaChatConfig;
    historyLength: number;
  } {
    return {
      initialized: this.isInitialized,
      config: this.config,
      historyLength: this.conversationHistory.length
    };
  }
}

export default GigaChatService;
