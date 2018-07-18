#pragma once

/**
 * Simulator class, which runs the simulation (on a different thread)
 * StateManager class should invoke this class with the heap_matrix of underlying data (?)
 * All communication with the simulation engine should use this class
 */

#include <thread>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <array>
#include <tuple>
#include <vector>
#include <algorithm>
#include <cstddef>

#include "canvasstate.hpp"
#include "heap_matrix.hpp"
#include "communicator.hpp"
#include "screencommunicator.hpp"
#include "concurrent_queue.hpp"


class Simulator {
public:
    using period_t = std::chrono::steady_clock::duration;
private:
    template <typename T>
    struct SizedArray {
        T* data;
        size_t size;
        SizedArray() noexcept : size(0) {}
        SizedArray(const SizedArray&) = delete;
        SizedArray& operator=(const SizedArray&) = delete;
        SizedArray(SizedArray&& other) noexcept {
            data = other.data;
            size = other.size;
            other.size = 0;
        };
        SizedArray& operator=(SizedArray&& other) noexcept {
            if (size > 0) {
                delete[] data;
            }
            data = other.data;
            size = other.size;
            other.size = 0;
        }
        ~SizedArray() noexcept {
            if (size > 0) {
                delete[] data;
            }
        }
        void resize(size_t newSize) {
            if (size > 0) {
                delete[] data;
            }
            size = newSize;
            if (size > 0) {
                data = new T[size];
            }
        }
        void update(const std::vector<T>& newData) {
            resize(newData.size());
            std::copy(newData.begin(), newData.end(), data);
        }
        void update(std::vector<T>&& newData) {
            resize(newData.size());
            std::move(newData.begin(), newData.end(), data);
        }
        T* begin() noexcept {
            return data;
        }
        T* end() noexcept {
            return data + size;
        }
        const T* begin() const noexcept {
            return data;
        }
        const T* end() const noexcept {
            return data + size;
        }
        T& operator[](size_t size) noexcept {
            return data[size];
        }
        const T& operator[](size_t size) const noexcept {
            return data[size];
        }
    };
    struct DynamicData;
    struct StaticData;
    struct SimulatorSource {
        int32_t outputComponent;
        inline void operator()(const DynamicData& oldData, DynamicData& newData) const noexcept;
    };
    template <size_t NumInputs>
    struct SimulatorLogicGate {
        std::array<int32_t, NumInputs> inputComponents;
        int32_t outputComponent;
    };
    template <size_t NumInputs>
    struct SimulatorAndGate : public SimulatorLogicGate<NumInputs> {
        inline void operator()(const DynamicData& oldData, DynamicData& newData) const noexcept;
    };
    template <size_t NumInputs>
    struct SimulatorOrGate : public SimulatorLogicGate<NumInputs> {
        inline void operator()(const DynamicData& oldData, DynamicData& newData) const noexcept;
    };
    template <size_t NumInputs>
    struct SimulatorNandGate : public SimulatorLogicGate<NumInputs> {
        inline void operator()(const DynamicData& oldData, DynamicData& newData) const noexcept;
    };
    template <size_t NumInputs>
    struct SimulatorNorGate : public SimulatorLogicGate<NumInputs> {
        inline void operator()(const DynamicData& oldData, DynamicData& newData) const noexcept;
    };
    template <template <size_t> typename Gate>
    struct GatePack {
        std::tuple<SizedArray<Gate<0>>, SizedArray<Gate<1>>, SizedArray<Gate<2>>, SizedArray<Gate<3>>, SizedArray<Gate<4>>> data;
        template <typename Callback>
        void forEach(Callback callback) const noexcept {
            callback(std::get<0>(data));
            callback(std::get<1>(data));
            callback(std::get<2>(data));
            callback(std::get<3>(data));
            callback(std::get<4>(data));
        }
    };
    struct Gates {
        GatePack<SimulatorAndGate> andGate;
        GatePack<SimulatorOrGate> orGate;
        GatePack<SimulatorNandGate> nandGate;
        GatePack<SimulatorNorGate> norGate;
        template <typename Callback>
        void forEach(Callback callback) const noexcept {
            callback(andGate);
            callback(orGate);
            callback(nandGate);
            callback(norGate);
        }
    };
    template <size_t NumInputs>
    struct SimulatorRelay {
        std::array<int32_t, NumInputs> inputComponents;
        int32_t outputRelayPixel;
    };
    template <size_t NumInputs>
    struct SimulatorPositiveRelay : public SimulatorRelay<NumInputs> {
        inline void operator()(const DynamicData& oldData, DynamicData& newData) const noexcept;
    };
    template <size_t NumInputs>
    struct SimulatorNegativeRelay : public SimulatorRelay<NumInputs> {
        inline void operator()(const DynamicData& oldData, DynamicData& newData) const noexcept;
    };
    template <template <size_t> typename Relay>
    struct RelayPack {
        std::tuple<SizedArray<Relay<0>>, SizedArray<Relay<1>>, SizedArray<Relay<2>>, SizedArray<Relay<3>>, SizedArray<Relay<4>>> data;
        template <typename Callback>
        void forEach(Callback callback) const noexcept {
            callback(std::get<0>(data));
            callback(std::get<1>(data));
            callback(std::get<2>(data));
            callback(std::get<3>(data));
            callback(std::get<4>(data));
        }
    };
    struct Relays {
        RelayPack<SimulatorPositiveRelay> positiveRelay;
        RelayPack<SimulatorNegativeRelay> negativeRelay;
        template <typename Callback>
        void forEach(Callback callback) const noexcept {
            callback(positiveRelay);
            callback(negativeRelay);
        }
    };
    struct SimulatorCommunicator {
        std::vector<int32_t> inputComponents;
        int32_t outputComponent;
        Communicator* communicator;
        inline void operator()(const DynamicData& oldData, DynamicData& newData, bool& transmitOutput) const noexcept;
    };
    struct RelayPixel {
        std::array<int32_t, 4> adjComponents;
        uint8_t numAdjComponents;
    };
    struct Component {
        int32_t adjRelayPixelsBegin;
        int32_t adjRelayPixelsEnd;
    };
    
    struct StaticData {
        // for all data that does not change after compilation

        // data about sources
        SizedArray<SimulatorSource> sources;

        // data about which component each gate maps to
        Gates logicGates;

        // data about which RelayPixel each relay maps to
        Relays relays;

        // data about communicators
        SizedArray<SimulatorCommunicator> communicators;

        int32_t screenCommunicatorStartIndex, screenCommunicatorEndIndex;

        // list of components
        SizedArray<Component> components;
        // list of relay pixels (relay pixels have one-to-one correspondence to relays)
        SizedArray<RelayPixel> relayPixels;
        // adj component list
        SizedArray<int32_t> adjComponentList;

        struct DisplayedPixel {
            enum struct PixelType : unsigned char {
                EMPTY,
                COMPONENT,
                RELAY,
                COMMUNICATOR
            } type;
            int32_t index[2]; // index[1] used only if it is an insulated wire with two directions
            inline bool logicLevel(const DynamicData&) const noexcept;
        };
        
        // state mapping
        ext::heap_matrix<DisplayedPixel> pixels;

        // sizes
        /*int32_t numComponents;
        int32_t numRelayPixels;*/
    };
    struct DynamicData {
        // for things that change at every simulation step

        // array of logic level of each connected component
        std::unique_ptr<bool[]> componentLogicLevels;
        // array of logic level of each relay pixel
        std::unique_ptr<bool[]> relayPixelLogicLevels;
        // array of whether each relay pixel is conductive
        std::unique_ptr<bool[]> relayPixelIsConductive;
        // array of whether each communicator is transmitting at HIGH
        std::unique_ptr<bool[]> communicatorTransmitStates;


        DynamicData() = delete;
        DynamicData(const DynamicData&) = delete;
        DynamicData& operator=(const DynamicData&) = delete;
        DynamicData(DynamicData&&) = default;
        DynamicData& operator=(DynamicData&&) = default;
        DynamicData(int32_t numComponents, int32_t numRelayPixels, int32_t numCommunicators) {
            componentLogicLevels = std::make_unique<bool[]>(numComponents);
            std::fill_n(componentLogicLevels.get(), numComponents, false);
            relayPixelLogicLevels = std::make_unique<bool[]>(numRelayPixels);
            std::fill_n(relayPixelLogicLevels.get(), numRelayPixels, false);
            relayPixelIsConductive = std::make_unique<bool[]>(numRelayPixels);
            std::fill_n(relayPixelIsConductive.get(), numRelayPixels, false);
            communicatorTransmitStates = std::make_unique<bool[]>(numCommunicators);
            std::fill_n(communicatorTransmitStates.get(), numCommunicators, false);
        }
    };
    /*struct CommunicatorInput {
        // this is a bit field
        // state : queue of states for communicator (up to 5 in queue)
        // count : number of states in queue excluding the first (live) one
        uint8_t state : 5, count : 3;
    };*/
    //using CommunicatorReceivedData = std::unique_ptr<CommunicatorInput[]>;
    friend struct CompilerSources;
    friend struct CompilerGates;
    friend struct CompilerRelays;
    friend struct CompilerStaticData;

    // The thread on which the simulation will run.
    std::thread simThread;

    // the static data
    StaticData staticData;

    // received communicator data (array of whether each communicator is receiving HIGH)
    // overwritten before every step
    // prepared and initialized by Simulator::compile()
    //CommunicatorReceivedData communicatorReceivedData;

    // The last CanvasState that is completely calculated.  This object might be accessed by the UI thread (for rendering purposes), but is updated (atomically) by the simulation thread.
    std::shared_ptr<DynamicData> latestCompleteState; // note: in C++20 this should be changed to std::atomic<std::shared_ptr<CanvasState>>.
    std::atomic<bool> simStopping; // flag for the UI thread to tell the simulation thread to stop.

    // synchronization stuff to wake the simulator thread if its sleeping
    std::mutex simSleepMutex;
    std::condition_variable simSleepCV;

    // minimum time between successive simulation steps (zero = as fast as possible)
    std::atomic<period_t::rep> period_rep; // it is stored using the underlying integer type so that we can use atomics
    std::chrono::steady_clock::time_point nextStepTime; // next time the simulator will be stepped

    // screen communicator input queue
    ext::concurrent_queue<ScreenInputCommunicatorEvent> screenInputQueue;

    /**
     * Method that actually runs the simulator.
     * Must be invoked from the simulator thread only!
     * This method does not return until the simulation is stopped.
     * @pre simulation has been compiled.
     */
    void run();

    /**
    * Method that calculates a single step of the simulation.
    * May be invoked in the simulator thread (by run()) or from the main thread (by step()).
    * This method is static, and hence is safe be called concurrently from multiple threads.
    * @pre simulation has been compiled.
    */
    void calculate(const StaticData& staticData, const DynamicData& oldState, DynamicData& newState);

    static void floodFill(const StaticData& staticData, DynamicData& dynamicData);

    void pullCommunicatorReceivedData();

public:

    ~Simulator();

    /**
     * Compiles the given gamestate and save the compiled simulation state (but does not start running the simulation).
     * Even though the simulator is stopped, those things that propagate immediately will be updated immediately (back to the CanvasState parameter).
     * @pre simulation is currently stopped.
     */
    void compile(CanvasState& gameState);

    /**
     * Resets the transient state and communicator data, and calls compile().
     * Even though the simulator is stopped, those things that propagate immediately will be updated immediately (back to the CanvasState parameter).
     * @pre simulation is currently stopped.
     */
    void reset(CanvasState& gameState);

    /**
     * Start running the simulation.
     * @pre simulation is currently stopped.
     */
    void start();

    /**
     * Stops (i.e. pauses) the simulation.
     * @pre simulation is currently running.
     */
    void stop();

    /**
    * Runs a single step of the simulation, then stops the simulation.
    * This method will block until the step is completed and the simulation is stopped.
    * @pre simulation is currently stopped.
    * @post simulation is stopped.
    */
    void step();

    /**
     * Returns true if the simulation is currently running, false otherwise.
     */
    bool running() const {
        return simThread.joinable();
    }

    /**
     * Returns true is the simulator currently holds a compiled simulation.
     */
    bool holdsSimulation() const {
        std::shared_ptr<DynamicData> tmpState = std::atomic_load_explicit(&latestCompleteState, std::memory_order_acquire);
        return tmpState != nullptr;
    }

    /**
     * Clears any active simulation.
     * @pre simulation is currently stopped.
     */
    void clear() {
        latestCompleteState = nullptr;
    }

    /**
     * Take a "snapshot" of the current simulation state and writes it to the argument supplied.
     * This works regardless whether the simulation is running or stopped.
     * @pre the supplied canvas state has the correct element positions as the canvas state that was compiled.
     */
    void takeSnapshot(CanvasState&) const;

    /**
     * Gets this period of the simulation step.
     * This works regardless whether the simulation is running or stopped.
     */
    std::chrono::steady_clock::duration getPeriod() const {
        return std::chrono::steady_clock::duration(period_rep.load(std::memory_order_acquire));
    }

    /**
    * Gets this period of the simulation step.
    * This works regardless whether the simulation is running or stopped.
    */
    void setPeriod(const std::chrono::steady_clock::duration& period) {
        period_rep.store(period.count(), std::memory_order_release);
    }

    void sendCommunicatorEvent(int32_t communicatorIndex, bool turnOn) {
        screenInputQueue.push(ScreenInputCommunicatorEvent{ communicatorIndex, turnOn });
    }
};
