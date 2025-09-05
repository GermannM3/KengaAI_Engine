import random
import time
import os

class KengaQuestText:
    def __init__(self):
        self.player_health = 100
        self.player_ammo = 30
        self.level = 1
        self.keys_collected = 0
        self.enemies_defeated = 0
        self.position = [0, 0]  # x, z координаты
        
    def clear_screen(self):
        os.system('cls' if os.name == 'nt' else 'clear')
        
    def show_logo(self):
        self.clear_screen()
        print("""
    ****************************************************
    *                                                  *
    *  KENGA QUEST                                     *
    *                                                  *
    *  A text-based demo of the KengaAI Engine game    *
    *                                                  *
    ****************************************************
        """)
        print("                    Demo Version")
        print("\nPress Enter to continue...")
        input()
        
    def show_main_menu(self):
        self.clear_screen()
        print("\nMain Menu:")
        print("1. New Game")
        print("2. Select Level")
        print("3. Exit")
        
        choice = input("\nChoose an action: ")
        return choice
        
    def show_level_menu(self):
        self.clear_screen()
        print("\nLevel Selection:")
        print("1. Level 1: Introduction")
        print("2. Level 2: Enemies")
        print("3. Back")
        
        choice = input("\nChoose a level: ")
        return choice
        
    def start_level_1(self):
        self.clear_screen()
        print("\n" + "="*50)
        print("LEVEL 1: INTRODUCTION")
        print("="*50)
        print("You are in a maze. Your task is to find the exit.")
        print("Use commands to navigate.")
        print("\nHealth:", self.player_health)
        print("Ammo:", self.player_ammo)
        print("Keys:", self.keys_collected)
        print("\nPosition:", self.position)
        
        while True:
            print("\nAvailable actions:")
            print("1. Go north (n)")
            print("2. Go south (s)")
            print("3. Go east (e)")
            print("4. Go west (w)")
            print("5. Look around")
            print("6. Exit level")
            
            action = input("\nEnter action: ").lower().strip()
            
            if action in ['1', 'n']:
                self.position[1] += 1
                print(f"You go north. New position: {self.position}")
                self.random_event()
                
            elif action in ['2', 's']:
                self.position[1] -= 1
                print(f"You go south. New position: {self.position}")
                self.random_event()
                
            elif action in ['3', 'e']:
                self.position[0] += 1
                print(f"You go east. New position: {self.position}")
                self.random_event()
                
            elif action in ['4', 'w']:
                self.position[0] -= 1
                print(f"You go west. New position: {self.position}")
                self.random_event()
                
            elif action in ['5', 'look']:
                print("You look around...")
                time.sleep(1)
                events = [
                    "You see footprints on the floor.",
                    "The walls are covered with strange symbols.",
                    "You hear footsteps somewhere in the distance.",
                    "There's a sign on the wall: 'Exit at [5,5]'.",
                    "You find an empty box.",
                    "The floor is illuminated by flickering lights."
                ]
                print(random.choice(events))
                
            elif action in ['6', 'exit']:
                print("Exiting level...")
                break
                
            else:
                print("Unknown command. Try again.")
                
            # Check if player reached the exit
            if self.position == [5, 5]:
                print("\n" + "!"*50)
                print("CONGRATULATIONS! You found the exit from the maze!")
                print("!"*50)
                time.sleep(3)
                break
                
    def start_level_2(self):
        self.clear_screen()
        print("\n" + "="*50)
        print("LEVEL 2: ENEMIES")
        print("="*50)
        print("Now there are enemies in the maze!")
        print("Be careful and use your weapon.")
        print("\nHealth:", self.player_health)
        print("Ammo:", self.player_ammo)
        print("Keys:", self.keys_collected)
        print("\nPosition:", self.position)
        
        enemies_encountered = 0
        
        while True:
            print("\nAvailable actions:")
            print("1. Go north (n)")
            print("2. Go south (s)")
            print("3. Go east (e)")
            print("4. Go west (w)")
            print("5. Look around")
            print("6. Shoot")
            print("7. Exit level")
            
            action = input("\nEnter action: ").lower().strip()
            
            if action in ['1', 'n']:
                self.position[1] += 1
                print(f"You go north. New position: {self.position}")
                self.random_combat_event()
                
            elif action in ['2', 's']:
                self.position[1] -= 1
                print(f"You go south. New position: {self.position}")
                self.random_combat_event()
                
            elif action in ['3', 'e']:
                self.position[0] += 1
                print(f"You go east. New position: {self.position}")
                self.random_combat_event()
                
            elif action in ['4', 'w']:
                self.position[0] -= 1
                print(f"You go west. New position: {self.position}")
                self.random_combat_event()
                
            elif action in ['5', 'look']:
                print("You look around...")
                time.sleep(1)
                events = [
                    "You see bloodstains on the floor.",
                    "The walls are scratched.",
                    "You hear angry growls in the distance.",
                    "There's a sign on the wall: 'Exit at [10,10]'.",
                    "You find a medkit (+20 health).",
                    "You find ammo (+10 bullets)."
                ]
                event = random.choice(events)
                print(event)
                if "medkit" in event:
                    self.player_health = min(100, self.player_health + 20)
                    print(f"Health restored! Current health: {self.player_health}")
                elif "ammo" in event:
                    self.player_ammo += 10
                    print(f"Ammo found! Current ammo: {self.player_ammo}")
                    
            elif action in ['6', 'shoot']:
                if self.player_ammo > 0:
                    self.player_ammo -= 1
                    print(f"You shoot! Ammo left: {self.player_ammo}")
                    if random.random() < 0.3:  # 30% chance to hit
                        print("You hit the enemy!")
                        self.enemies_defeated += 1
                        print(f"Total enemies defeated: {self.enemies_defeated}")
                    else:
                        print("Miss!")
                else:
                    print("You're out of ammo!")
                    
            elif action in ['7', 'exit']:
                print("Exiting level...")
                break
                
            else:
                print("Unknown command. Try again.")
                
            # Check if player reached the exit
            if self.position == [10, 10]:
                print("\n" + "!"*50)
                print("CONGRATULATIONS! You found the exit from the maze!")
                print("You defeated all enemies and completed the level!")
                print("!"*50)
                time.sleep(3)
                break
                
    def random_event(self):
        """Random events for the first level"""
        if random.random() < 0.1:  # 10% chance of event
            events = [
                "You find a key!",
                "You hear a strange sound.",
                "The floor suddenly lights up brightly.",
                "You find a small medkit (+10 health)."
            ]
            event = random.choice(events)
            print(f"\n[EVENT] {event}")
            
            if "key" in event:
                self.keys_collected += 1
                print(f"Keys: {self.keys_collected}")
            elif "medkit" in event:
                self.player_health = min(100, self.player_health + 10)
                print(f"Health: {self.player_health}")
                
            time.sleep(1)
            
    def random_combat_event(self):
        """Random combat events for the second level"""
        if random.random() < 0.15:  # 15% chance of event
            if random.random() < 0.7:  # 70% chance of enemy encounter
                print("\n[COMBAT] You encounter an enemy!")
                enemy_health = random.randint(20, 40)
                print(f"Enemy health: {enemy_health}")
                
                while enemy_health > 0 and self.player_health > 0:
                    print(f"\nYour health: {self.player_health}")
                    print(f"Enemy health: {enemy_health}")
                    print("1. Shoot")
                    print("2. Run away")
                    
                    choice = input("Choose action: ")
                    
                    if choice == '1':
                        if self.player_ammo > 0:
                            self.player_ammo -= 1
                            damage = random.randint(15, 30)
                            enemy_health -= damage
                            print(f"You deal {damage} damage! Ammo left: {self.player_ammo}")
                            
                            if enemy_health <= 0:
                                print("Enemy defeated!")
                                self.enemies_defeated += 1
                                reward = random.randint(5, 15)
                                self.player_health = min(100, self.player_health + reward)
                                print(f"You gain {reward} health as reward!")
                                break
                            else:
                                # Enemy attacks
                                enemy_damage = random.randint(5, 15)
                                self.player_health -= enemy_damage
                                print(f"Enemy deals you {enemy_damage} damage!")
                                
                                if self.player_health <= 0:
                                    print("You died in combat...")
                                    return
                        else:
                            print("You're out of ammo!")
                            
                    elif choice == '2':
                        if random.random() < 0.5:  # 50% chance to escape
                            print("You successfully escaped from the enemy!")
                            break
                        else:
                            print("Failed to escape!")
                            enemy_damage = random.randint(5, 15)
                            self.player_health -= enemy_damage
                            print(f"Enemy deals you {enemy_damage} damage while you try to escape!")
                            
                            if self.player_health <= 0:
                                print("You died in combat...")
                                return
                    else:
                        print("Invalid choice!")
                        
            else:
                # Positive events
                events = [
                    "You find a key!",
                    "You find a medkit (+20 health).",
                    "You find ammo (+15).",
                    "You find an enhanced medkit (+30 health)."
                ]
                event = random.choice(events)
                print(f"\n[FIND] {event}")
                
                if "key" in event:
                    self.keys_collected += 1
                    print(f"Keys: {self.keys_collected}")
                elif "medkit" in event:
                    heal_amount = 20 if "enhanced" not in event else 30
                    self.player_health = min(100, self.player_health + heal_amount)
                    print(f"Health: {self.player_health}")
                elif "ammo" in event:
                    self.player_ammo += 15
                    print(f"Ammo: {self.player_ammo}")
                    
                time.sleep(1)
                
    def run(self):
        self.show_logo()
        
        while True:
            choice = self.show_main_menu()
            
            if choice == '1':
                print("\nStarting new game...")
                time.sleep(1)
                self.__init__()  # Reset game
                self.start_level_1()
                
            elif choice == '2':
                while True:
                    level_choice = self.show_level_menu()
                    
                    if level_choice == '1':
                        self.start_level_1()
                    elif level_choice == '2':
                        self.start_level_2()
                    elif level_choice == '3':
                        break
                    else:
                        print("Invalid choice!")
                        
            elif choice == '3':
                print("\nThanks for playing!")
                break
                
            else:
                print("Invalid choice!")

if __name__ == "__main__":
    game = KengaQuestText()
    game.run()