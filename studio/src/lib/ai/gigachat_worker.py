#!/usr/bin/env python3
"""
GigaChat Worker Script
Python процесс для работы с моделью GigaChat в KengaAI Studio
"""

import sys
import json
import os
import torch
from transformers import AutoTokenizer, AutoModelForCausalLM, GenerationConfig
import traceback
import logging

# Настройка логирования
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class GigaChatWorker:
    """Worker класс для работы с моделью GigaChat"""

    def __init__(self, model_path: str = "models/GigaChat-20B-A3B-base", device: str = "auto"):
        self.model_path = model_path
        self.device = device if device != "auto" else ("cuda" if torch.cuda.is_available() else "cpu")
        self.model = None
        self.tokenizer = None
        self.is_loaded = False

        logger.info(f"Инициализация GigaChat worker на устройстве: {self.device}")

    def load_model(self):
        """Загрузка модели"""
        try:
            logger.info("Загрузка токенизатора...")
            self.tokenizer = AutoTokenizer.from_pretrained(
                self.model_path,
                trust_remote_code=True
            )

            logger.info("Загрузка модели...")
            # Оптимизации для разных устройств
            if self.device == "cuda":
                # Использование 8-bit quantization для экономии памяти
                self.model = AutoModelForCausalLM.from_pretrained(
                    self.model_path,
                    torch_dtype=torch.bfloat16,
                    device_map="auto",
                    trust_remote_code=True,
                    load_in_8bit=True,
                    max_memory={0: "12GB", "cpu": "8GB"}  # Ограничение памяти
                )
            else:
                # CPU оптимизации
                self.model = AutoModelForCausalLM.from_pretrained(
                    self.model_path,
                    torch_dtype=torch.float32,
                    device_map={"": "cpu"},
                    trust_remote_code=True,
                    low_cpu_mem_usage=True
                )

            # Загрузка конфигурации генерации
            try:
                self.model.generation_config = GenerationConfig.from_pretrained(self.model_path)
            except:
                logger.warning("Не удалось загрузить generation config, используем по умолчанию")

            self.is_loaded = True
            logger.info("✅ Модель успешно загружена")

        except Exception as e:
            logger.error(f"Ошибка загрузки модели: {e}")
            raise

    def generate(self, prompt: str, max_tokens: int = 2048, temperature: float = 0.7) -> str:
        """Генерация текста"""
        if not self.is_loaded:
            raise RuntimeError("Модель не загружена")

        try:
            logger.info(f"Генерация ответа (макс. токенов: {max_tokens}, температура: {temperature})")

            # Токенизация
            inputs = self.tokenizer(prompt, return_tensors="pt")

            # Перенос на устройство модели
            if self.device == "cuda":
                inputs = inputs.to(self.model.device)
            else:
                inputs = inputs.to("cpu")

            # Генерация с оптимизациями
            with torch.no_grad():
                outputs = self.model.generate(
                    **inputs,
                    max_new_tokens=max_tokens,
                    temperature=temperature,
                    do_sample=True,
                    pad_token_id=self.tokenizer.eos_token_id,
                    eos_token_id=self.tokenizer.eos_token_id,
                    repetition_penalty=1.1,  # Избегание повторений
                    no_repeat_ngram_size=3,  # Блокировка повторяющихся n-gram
                    early_stopping=True
                )

            # Декодирование ответа
            response = self.tokenizer.decode(
                outputs[0][inputs.input_ids.shape[1]:],
                skip_special_tokens=True
            ).strip()

            logger.info(f"Сгенерирован ответ длиной {len(response)} символов")
            return response

        except Exception as e:
            error_msg = f"Ошибка генерации: {str(e)}"
            logger.error(error_msg)
            logger.error(traceback.format_exc())
            return error_msg

    def get_model_info(self) -> dict:
        """Получение информации о модели"""
        if not self.is_loaded:
            return {"status": "not_loaded"}

        return {
            "status": "loaded",
            "device": self.device,
            "model_path": self.model_path,
            "vocab_size": self.tokenizer.vocab_size if self.tokenizer else None,
            "max_position_embeddings": getattr(self.model.config, 'max_position_embeddings', None),
            "torch_version": torch.__version__,
            "cuda_available": torch.cuda.is_available(),
            "cuda_version": torch.version.cuda if torch.cuda.is_available() else None,
            "gpu_name": torch.cuda.get_device_name() if torch.cuda.is_available() else None,
            "memory_allocated": torch.cuda.memory_allocated() / 1024**3 if torch.cuda.is_available() else None
        }

def main():
    """Основная функция"""
    logger.info("Запуск GigaChat Worker")

    # Получение пути к модели из аргументов или переменной окружения
    model_path = sys.argv[1] if len(sys.argv) > 1 else os.getenv("GIGACHAT_MODEL_PATH", "models/GigaChat-20B-A3B-base")
    device = os.getenv("GIGACHAT_DEVICE", "auto")

    try:
        # Создание worker
        worker = GigaChatWorker(model_path, device)

        # Загрузка модели
        worker.load_model()

        logger.info("Worker готов к обработке запросов")

        # Основной цикл обработки
        for line in sys.stdin:
            try:
                data = json.loads(line.strip())
                action = data.get("action")

                if action == "generate":
                    response = worker.generate(
                        data["prompt"],
                        data.get("max_tokens", 2048),
                        data.get("temperature", 0.7)
                    )
                    result = {
                        "response": response,
                        "status": "success",
                        "timestamp": json.dumps({"$date": {"$numberLong": str(int(os.times().elapsed * 1000))}})
                    }

                elif action == "get_info":
                    result = worker.get_model_info()
                    result["status"] = "success"

                elif action == "ping":
                    result = {"status": "pong", "timestamp": json.dumps({"$date": {"$numberLong": str(int(os.times().elapsed * 1000))}})}

                else:
                    result = {
                        "error": f"Неизвестное действие: {action}",
                        "status": "error"
                    }

                # Отправка результата
                print(json.dumps(result, ensure_ascii=False))
                sys.stdout.flush()

            except json.JSONDecodeError as e:
                error_result = {
                    "error": f"Ошибка парсинга JSON: {str(e)}",
                    "status": "error"
                }
                print(json.dumps(error_result, ensure_ascii=False))
                sys.stdout.flush()

            except Exception as e:
                error_result = {
                    "error": f"Внутренняя ошибка: {str(e)}",
                    "status": "error"
                }
                logger.error(f"Ошибка обработки запроса: {e}")
                logger.error(traceback.format_exc())
                print(json.dumps(error_result, ensure_ascii=False))
                sys.stdout.flush()

    except KeyboardInterrupt:
        logger.info("Получен сигнал прерывания")
    except Exception as e:
        logger.error(f"Критическая ошибка: {e}")
        logger.error(traceback.format_exc())
        sys.exit(1)

if __name__ == "__main__":
    main()
