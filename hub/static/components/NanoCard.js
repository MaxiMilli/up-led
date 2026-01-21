export const NanoCard = {
   props: {
      mac: String,
      data: Object
   },

   data () {
      return {
         instruments: {
            'trumpet': '🎺',
            'drum': '🥁',
            'piano': '🎹'
         },
         colors: {
            'bg-blue-500': 'bg-blue-500',
            'bg-red-500': 'bg-red-500',
            'bg-green-500': 'bg-green-500',
            'bg-yellow-500': 'bg-yellow-500',
            'bg-purple-500': 'bg-purple-500'
         },
         showRegisterDialog: false,
         showLedDialog: false,
         registerMap: {
            '2': 'Drums Person',
            '4': 'Pauken Person',
            '5': 'Lira',
            '6': 'Chinellen',
            '7': '1. Trompete',
            '8': '2. Trompete',
            '9': '1. Posaune',
            '10': '2. Posaune',
            '11': '3. Posaune',
            '12': 'Bässe',
            '13': 'Bässe Instrumente',
            '14': 'Wägeli Instrumente',
            '15': 'Pauken Instrumente'
         }
      }
   },

   computed: {
      timeSinceLastSeen () {
         const lastSeen = new Date(this.data.last_seen)
         return Math.round((new Date() - lastSeen) / 1000)
      },
      registers () {
         return Object.entries(this.registerMap)
      },

      registerLabel () {
         return this.registerMap[this.data.register] || this.data.register
      }
   },

   methods: {
      async updateName () {
         const newName = prompt("Enter new name:")
         if (newName === null) return

         try {
            await this.updateNano({ name: newName })
            this.$emit('refresh')
         } catch (error) {
            console.error('Error updating nano name:', error)
            alert('Failed to update name')
         }
      },

      async updateInstrument (instrument) {
         try {
            await this.updateNano({ instrument })
            this.$emit('refresh')
         } catch (error) {
            console.error('Error updating nano instrument:', error)
            alert('Failed to update instrument')
         }
      },

      async updateColor (color) {
         try {
            await this.updateNano({ color })
            this.$emit('refresh')
         } catch (error) {
            console.error('Error updating nano color:', error)
            alert('Failed to update color')
         }
      },

      async updateGwaendliColor (gwaendli_color) {
         try {
            await this.updateNano({ gwaendli_color })
            this.$emit('refresh')
         } catch (error) {
            console.error('Error updating nano color:', error)
            alert('Failed to update color')
         }
      },

      async updateNano (data) {
         const response = await fetch(`/nano/update/${this.mac}`, {
            method: 'POST',
            headers: {
               'Content-Type': 'application/json',
            },
            body: JSON.stringify(data)
         })

         if (!response.ok) throw new Error('Update failed')
      },

      updateRegister () {
         this.showRegisterDialog = true
      },

      async selectRegister (register) {
         try {
            await this.updateNano({ register })
            this.$emit('refresh')
         } catch (error) {
            console.error('Error updating nano register:', error)
            alert('Failed to update register')
         } finally {
            this.showRegisterDialog = false
         }
      },

      closeRegisterDialog () {
         this.showRegisterDialog = false
      },

      updateLedCount () {
         this.showLedDialog = true
      },

      async selectLedCount (count) {
         try {
            await this.updateNano({ led_count: count })
            this.$emit('refresh')
         } catch (error) {
            console.error('Error updating LED count:', error)
            alert('Failed to update LED count')
         } finally {
            this.showLedDialog = false
         }
      },

      closeLedDialog () {
         this.showLedDialog = false
      }
   },

   template: `
        <div class="nano-card p-4 bg-white rounded-lg shadow min-w-96 dark:bg-gray-800" :class="[data.status === 'active' ? 'opacity-100' : 'opacity-50']" :data-mac="mac">
            <div class="flex items-center justify-between mb-2">
                <div class="flex items-center">
                    <div class="w-3 h-3" 
                         :class="[data.status === 'active' ? 'bg-green-500' : 'bg-red-500', 'rounded-full mr-2']">
                    </div>
                    <h4 class="font-medium text-gray-900 dark:text-white cursor-pointer hover:text-blue-500" 
                        @click="updateName">{{ data.name || "(no name)" }}</h4>
                </div>
                <div class="text-xs text-gray-500 text-right mb-2">{{ mac }}</div>
            </div>
            <div class="flex items-center justify-between">
                <div class="text-sm text-gray-500 dark:text-gray-400">
                    <p>IP: {{ data.ip }}</p>
                    <p>Last seen: {{ timeSinceLastSeen }}s ago</p>
                </div>
                <div class="flex gap-1">
                    <button
                        class="bg-gray-200 text-black px-4 py-2 rounded hover:bg-gray-600 hover:text-white"
                        @click="updateLedCount"
                    >
                        {{ data.led_count || 0 }}
                    </button>
                    <button
                        class="bg-gray-200 text-black px-4 py-2 rounded hover:bg-gray-600 hover:text-white"
                        @click="updateRegister"
                    >
                        {{ registerLabel }}
                    </button>
                </div>
                
            </div>
            <div class="mt-4 flex justify-between items-center">
                <div class="flex gap-2 text-2xl">
                    <span v-for="(emoji, key) in instruments" 
                          :key="key"
                          class="cursor-pointer w-12 h-12 flex items-center justify-center border-2 border-gray-300 rounded-full hover:opacity-100"
                          :class="[data.instrument === key ? 'opacity-100 border-black' : 'opacity-50']"
                          @click="updateInstrument(key)">
                        {{ emoji }}
                    </span>
                </div>
                <div class="flex items-center justify-center gap-2">
                    <div v-for="color in ['cyan', 'magenta', 'yellow']"
                         :key="color"
                         class="text-white px-4 py-2 rounded-full cursor-pointer w-12 h-12 flex justify-center items-center"
                         :class="[
                            color === 'cyan' ? 'bg-cyan-500' : '',
                            color === 'magenta' ? 'bg-pink-500' : '',
                            color === 'yellow' ? 'bg-yellow-500' : '',
                            data.gwaendli_color === color ? 'opacity-100' : 'opacity-20'
                         ]"
                         @click="updateGwaendliColor(color)">
                        {{ color[0].toUpperCase() }}
                    </div>
                </div>
            </div>
            <div v-if="showRegisterDialog" class="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
                <div class="bg-white dark:bg-gray-800 p-6 rounded-lg shadow-lg max-w-2xl w-full scrollbar-thin scrollbar-thumb-gray-300 scrollbar-track-gray-100 h-[80vh] overflow-y-auto">
                    <div class="grid grid-cols-3 gap-4">
                        <button v-for="[value, label] in registers" 
                                :key="value"
                                @click="selectRegister(value)"
                                class="px-4 py-2 border border-gray-300 rounded hover:bg-gray-100 dark:hover:bg-gray-700 dark:text-white">
                            {{ label }}
                        </button>
                    </div>
                    <div class="mt-4 text-right">
                        <button @click="closeRegisterDialog" 
                                class="px-4 py-2 text-gray-500 hover:text-gray-700 dark:text-gray-400 dark:hover:text-gray-200">
                            Schliessen
                        </button>
                    </div>
                </div>
            </div>
            <div v-if="showLedDialog" class="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
                <div class="bg-white dark:bg-gray-800 p-6 rounded-lg shadow-lg max-w-2xl w-full scrollbar-thin scrollbar-thumb-gray-300 scrollbar-track-gray-100 h-[80vh] overflow-y-auto">
                    <div class="grid grid-cols-5 gap-4 overflow-y-auto">
                        <button v-for="n in 150" 
                              :key="n"
                              @click="selectLedCount(n)"
                              class="px-4 py-2 border border-gray-300 rounded hover:bg-gray-100 dark:hover:bg-gray-700 dark:text-white">
                            {{ n }}
                        </button>
                    </div>
                    <div class="mt-4 text-right">
                        <button @click="closeLedDialog" 
                              class="px-4 py-2 text-gray-500 hover:text-gray-700 dark:text-gray-400 dark:hover:text-gray-200">
                            Schliessen
                        </button>
                    </div>
                </div>
            </div>
        </div>
    `
} 
