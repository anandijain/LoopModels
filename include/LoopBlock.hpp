#pragma once

#include "./ArrayReference.hpp"
#include "./BitSets.hpp"
#include "./DependencyPolyhedra.hpp"
#include "./Graphs.hpp"
#include "./LinearAlgebra.hpp"
#include "./Loops.hpp"
#include "./Macro.hpp"
#include "./Math.hpp"
#include "./NormalForm.hpp"
#include "./Orthogonalize.hpp"
#include "./Polyhedra.hpp"
#include "./Schedule.hpp"
#include "./Simplex.hpp"
#include "./Symbolics.hpp"
#include <bits/ranges_algo.h>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/User.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/Casting.h>

template <std::integral I>
[[maybe_unused]] static void insertSortedUnique(llvm::SmallVectorImpl<I> &v,
                                                const I &x) {
    for (auto it = v.begin(), ite = v.end(); it != ite; ++it) {
        if (*it < x)
            continue;
        if (*it > x)
            v.insert(it, x);
        return;
    }
    v.push_back(x);
}

struct ScheduledNode {
    // llvm::SmallVector<unsigned> parentNodes;
    // llvm::SmallVector<unsigned> childNodes;
    // llvm::SmallVector<unsigned> memory;
    BitSet memory;
    BitSet inNeighbors;
    BitSet outNeighbors;
    Schedule schedule;
    // llvm::SmallVector<MemoryAccess*> memory;
    static constexpr uint32_t PHISCHEDULEDFLAG =
        std::numeric_limits<uint32_t>::max();
    uint32_t phiOffset{0};   // used in LoopBlock
    uint32_t omegaOffset{0}; // used in LoopBlock
    uint8_t numLoops{0};
    bool visited{false};
    bool wasVisited() const { return visited; }
    void visit() { visited = true; }
    void unVisit() { visited = false; }
    size_t getNumLoops() const { return numLoops; }
    bool phiIsScheduled() const { return phiOffset == PHISCHEDULEDFLAG; }

    size_t updatePhiOffset(size_t p) {
        phiOffset = p;
        return p + numLoops;
    }
    size_t updateOmegaOffset(size_t o) {
        omegaOffset = o;
        return ++o;
    }
    Range<size_t, size_t> getPhiOffset() const {
        return _(phiOffset, phiOffset + numLoops);
    }

    ScheduledNode operator|(const ScheduledNode &s) const {
        uint8_t nL = std::max(numLoops, s.numLoops);
        return {memory | s.memory,
                (inNeighbors | s.inNeighbors),
                (outNeighbors | s.outNeighbors),
                Schedule(nL),
                0,
                0,
                nL};
    }
    ScheduledNode &operator|=(const ScheduledNode &s) {
        memory |= s.memory;
        outNeighbors |= s.outNeighbors;
        numLoops = std::max(numLoops, s.numLoops);
        return *this;
    }
};

// A loop block is a block of the program that may include multiple loops.
// These loops are either all executed (note iteration count may be 0, or
// loops may be in rotated form and the guard prevents execution; this is okay
// and counts as executed for our purposes here ), or none of them are.
// That is, the LoopBlock does not contain divergent control flow, or guards
// unrelated to loop bounds.
// The loops within a LoopBlock are optimized together, so we can consider
// optimizations such as reordering or fusing them together as a set.
//
//
// Initially, the `LoopBlock` is initialized as a set of
// `Read` and `Write`s, without any dependence polyhedra.
// Then, it builds `DependencePolyhedra`.
// These can be used to construct an ILP.
//
// That is:
// fields that must be provided/filled:
//  - refs
//  - memory
//  - userToMemory
// fields it self-initializes:
//
//
// NOTE: w/ respect to index linearization (e.g., going from Cartesian indexing
// to linear indexing), the current behavior will be to fully delinearize as a
// preprocessing step. Linear indexing may be used later as an optimization.
// This means that not only do we want to delinearize
// for (n = 0; n < N; ++n){
//   for (m = 0; m < M; ++m){
//      C(m + n*M)
//   }
// }
// we would also want to delinearize
// for (i = 0; i < M*N; ++i){
//   C(i)
// }
// into
// for (n = 0; n < N; ++n){
//   for (m = 0; m < M; ++m){
//      C(m, n)
//   }
// }
// and then relinearize as an optimization later.
// Then we can compare fully delinearized loop accesses.
// Should be in same block:
// s = 0
// for (i = eachindex(x)){
//   s += x[i]; // Omega = [0, _, 0]
// }
// m = s / length(x); // Omega = [1]
// for (i = eachindex(y)){
//   f(m, ...); // Omega = [2, _, 0]
// }
struct LoopBlock { // : BaseGraph<LoopBlock, ScheduledNode> {
    // llvm::SmallVector<ArrayReference, 0> refs;
    // TODO: figure out how to handle the graph's dependencies based on
    // operation/instruction chains.
    // Perhaps implicitly via the graph when using internal orthogonalization
    // and register tiling methods, and then generate associated constraints
    // or aliasing between schedules when running the ILP solver?
    // E.g., the `dstOmega[numLoopsCommon-1] > srcOmega[numLoopsCommon-1]`,
    // and all other other shared schedule parameters are aliases (i.e.,
    // identical)?
    // using VertexType = ScheduledNode;
    llvm::SmallVector<MemoryAccess, 0> memory;
    llvm::SmallVector<ScheduledNode, 0> nodes;
    // llvm::SmallVector<unsigned> memoryToNodeMap;
    llvm::SmallVector<Dependence, 0> edges;
    // llvm::SmallVector<bool> visited; // visited, for traversing graph
    llvm::DenseMap<llvm::User *, MemoryAccess *> userToMemory;
    llvm::SmallVector<Polynomial::Monomial> symbols;
    Simplex omniSimplex;
    // Simplex activeSimplex;
    // we may turn off edges because we've exceeded its loop depth
    // or because the dependence has already been satisfied at an
    // earlier level.
    // llvm::SmallVector<bool, 256> doNotAddEdge;
    llvm::SmallVector<bool, 256> scheduled;
    size_t numPhiCoefs{0};
    size_t numOmegaCoefs{0};
    size_t numLambda{0};
    size_t numBounding{0};
    size_t numConstraints{0};
    size_t numActiveEdges{0};
    // Simplex simplex;
    // ArrayReference &ref(MemoryAccess &x) { return refs[x.ref]; }
    // ArrayReference &ref(MemoryAccess *x) { return refs[x->ref]; }
    // const ArrayReference &ref(const MemoryAccess &x) const {
    //     return refs[x.ref];
    // }
    // const ArrayReference &ref(const MemoryAccess *x) const {
    //     return refs[x->ref];
    // }
    size_t numVerticies() const { return nodes.size(); }
    llvm::MutableArrayRef<ScheduledNode> getVerticies() { return nodes; }
    llvm::ArrayRef<ScheduledNode> getVerticies() const { return nodes; }
    struct OutNeighbors {
        LoopBlock &loopBlock;
        ScheduledNode &node;
        // size_t size()const{return node.num
    };
    OutNeighbors outNeighbors(size_t idx) {
        return OutNeighbors{*this, nodes[idx]};
    }
    // llvm::SmallVector<unsigned> &edgesOut(size_t idx) {
    //     return memory[idx].edgesOut;
    // }
    // const llvm::SmallVector<unsigned> &edgesOut(size_t idx) const {
    //     return memory[idx].edgesOut;
    // }
    [[nodiscard]] size_t calcMaxDepth() const {
        size_t d = 0;
        for (auto &mem : memory)
            d = std::max(d, mem.getNumLoops());
        return d;
    }
    [[nodiscard]] bool isSatisfied(const Dependence &e) const {
        const Simplex &sat = e.dependenceSatisfaction;
        Schedule &schIn = e.in->schedule;
        Schedule &schOut = e.out->schedule;
        const ArrayReference &refIn = e.in->ref;
        const ArrayReference &refOut = e.out->ref;
        size_t numLoopsIn = refIn.getNumLoops();
        size_t numLoopsOut = refOut.getNumLoops();
        size_t numLoopsCommon = std::min(numLoopsIn, numLoopsOut);
        size_t numLoopsTotal = numLoopsIn + numLoopsOut;
        Vector<int64_t> schv;
        schv.resizeForOverwrite(sat.getNumVar());
        const SquarePtrMatrix<int64_t> inPhi = schIn.getPhi();
        const SquarePtrMatrix<int64_t> outPhi = schOut.getPhi();
        llvm::ArrayRef<int64_t> inOmega = schIn.getOmega();
        llvm::ArrayRef<int64_t> outOmega = schOut.getOmega();
        const size_t numLambda = e.getNumLambda();
        // when i == numLoopsCommon, we've passed the last loop
        for (size_t i = 0; i <= numLoopsCommon; ++i) {
            if (int64_t o2idiff = outOmega[2 * i] - inOmega[2 * i]) {
                return (o2idiff > 0);
            }

            // we should not be able to reach `numLoopsCommon`
            // because at the very latest, this last schedule value
            // should be different, because either:
            // if (numLoopsX == numLoopsY){
            //   we're at the inner most loop, where one of the instructions
            //   must have appeared before the other.
            // } else {
            //   the loop nests differ in depth, in which case the deeper
            //   loop must appear either above or below the instructions
            //   present at that level
            // }
            assert(i != numLoopsCommon);
            const size_t offIn = e.forward ? 0 : numLoopsOut;
            const size_t offOut = e.forward ? numLoopsIn : 0;
            for (size_t j = 0; j < numLoopsIn; ++j) {
                schv[j + offIn] = inPhi(j, i);
            }
            for (size_t j = 0; j < numLoopsOut; ++j) {
                schv[j + offOut] = outPhi(j, i);
            }
            int64_t inO = inOmega[2 * i + 1], outO = outOmega[2 * i + 1];
            // forward means offset is 2nd - 1st
            schv[numLoopsTotal] = outO - inO;
            // dependenceSatisfaction is phi_t - phi_s >= 0
            // dependenceBounding is w + u'N - (phi_t - phi_s) >= 0
            // we implicitly 0-out `w` and `u` here,
            if (sat.satisfiable(schv, numLambda)) {
                if (e.dependenceBounding.unSatisfiable(schv, numLambda)) {
                    // if zerod-out bounding not >= 0, then that means
                    // phi_t - phi_s > 0, so the dependence is satisfied
                    return true;
                }
            } else {
                // if not satisfied, false
                return false;
            }
        }
        assert(false);
        return false;
    }

    // NOTE: this relies on two important assumptions:
    // 1. Code has been fully delinearized, so that axes all match
    //    (this means that even C[i], 0<=i<M*N -> C[m*M*n])
    //    (TODO: what if we have C[n+N*m] and C[m+M*n]???)
    //    (this of course means we have to see other uses in
    //     deciding whether to expand `C[i]`, and what to expand
    //     it into.)
    // 2. Reduction targets have been orthogonalized, so that
    //     the number of axes reflects the number of loops they
    //     depend on.
    // if we have
    // for (i = I, j = J, m = M, n = N) {
    //   C(m,n) = foo(C(m,n), ...)
    // }
    // then we have dependencies that
    // the load C(m,n) [ i = x, j = y ]
    // happens after the store C(m,n) [ i = x-1, j = y], and
    // happens after the store C(m,n) [ i = x, j = y-1]
    // and that the store C(m,n) [ i = x, j = y ]
    // happens after the load C(m,n) [ i = x-1, j = y], and
    // happens after the load C(m,n) [ i = x, j = y-1]
    //
    // so, `pushReductionEdges` will...
    // actually, probably better to put this into dependence checking
    // so that it can add optionally 0, 1, or 2 dependencies
    // void pushReductionEdges(MemoryAccess &x, MemoryAccess &y) {
    //     if (!x.fusedThrough(y)) {
    //         return;
    //     }
    //     ArrayReference &refX = x.ref;
    //     ArrayReference &refY = y.ref;
    //     const size_t numLoopsX = refX.getNumLoops();
    //     const size_t numLoopsY = refY.getNumLoops();
    //     const size_t numAxes = refX.dim();
    //     // we preprocess to delinearize all, including linear indexing
    //     assert(numAxes == refY.dim());
    //     const size_t numLoopsCommon = std::min(numLoopsX, numLoopsY);
    //     for (size_t i = numAxes; i < numLoopsCommon; ++i) {
    //         // push both edge directions
    //     }
    // }
    void addEdge(MemoryAccess &mai, MemoryAccess &maj) {
        // note, axes should be fully delinearized, so should line up
        // as a result of preprocessing.
        if (size_t numDeps = Dependence::check(edges, mai, maj)) {
            size_t numEdges = edges.size();
            size_t e = numEdges - numDeps;
            do {
                edges[e].in->addEdgeOut(e);
                edges[e].out->addEdgeIn(e);
            } while (++e < numEdges);
            //             Dependence &d(dep.getValue());
            // #ifndef NDEBUG
            //             if (d.isForward()) {
            //                 std::cout << "dep direction: x -> y" <<
            //                 std::endl;
            //             } else {
            //                 std::cout << "dep direction: y -> x" <<
            //                 std::endl;
            //             }
            // #endif
            //             MemoryAccess *pin, *pout;
            //             if (d.isForward()) {
            //                 pin = &mai;
            //                 pout = &maj;
            //             } else {
            //                 pin = &maj;
            //                 pout = &mai;
            //             }
            //             edges.emplace_back(std::move(d), pin, pout);
            //             // input's out-edge goes to output's in-edge
            //             pin->addEdgeOut(numEdges);
            //             pout->addEdgeIn(numEdges);
            //             // pushReductionEdges(mai, maj);
        }
    }
    // fills all the edges between memory accesses, checking for
    // dependencies.
    void fillEdges() {
        for (size_t i = 1; i < memory.size(); ++i) {
            MemoryAccess &mai = memory[i];
            ArrayReference &refI = mai.ref;
            // SHOWLN(i);
            for (size_t j = 0; j < i; ++j) {
                MemoryAccess &maj = memory[j];
                ArrayReference &refJ = maj.ref;
                // CSHOW(j);
                // CSHOW(refI.arrayID);
                // CSHOW(refJ.arrayID);
                // CSHOW(mai.isLoad);
                // CSHOW(maj.isLoad);
                if ((refI.arrayID != refJ.arrayID) ||
                    ((mai.isLoad) && (maj.isLoad)))
                    std::cout << std::endl;
                if ((refI.arrayID != refJ.arrayID) ||
                    ((mai.isLoad) && (maj.isLoad)))
                    continue;
                addEdge(mai, maj);
                // CSHOWLN(edges.size());
            }
        }
    }
    unsigned searchUsesForStores(llvm::User *u, unsigned nodeIndex,
                                 unsigned maxNode) {
        for (auto &use : u->uses()) {
            llvm::User *user = use.getUser();
            if (llvm::dyn_cast<llvm::StoreInst>(user) != nullptr) {
                auto memAccess = userToMemory.find(user);
                if (memAccess != userToMemory.end()) {
                    // already has a nodeIndex
                    unsigned oldNodeIndex = memAccess->getSecond()->nodeIndex;
                    if ((oldNodeIndex !=
                         std::numeric_limits<unsigned>::max()) &&
                        oldNodeIndex != nodeIndex) {
                        // merge nodeIndex and oldNodeIndex
                        if (oldNodeIndex < nodeIndex)
                            std::swap(nodeIndex, oldNodeIndex);
                        // delete oldNodeIndex
                        for (auto &&mem : memory) {
                            if (mem.nodeIndex == oldNodeIndex) {
                                mem.nodeIndex = nodeIndex;
                            } else if ((mem.nodeIndex > oldNodeIndex) &&
                                       (mem.nodeIndex !=
                                        std::numeric_limits<unsigned>::max())) {
                                --mem.nodeIndex;
                            }
                        }
                        --maxNode;
                    } else {
                        // userToMemory
                        memAccess->getSecond()->nodeIndex = nodeIndex;
                    }
                } else {
                    SHOWLN(user);
                    for (auto &mem : memory)
                        SHOWLN(mem.user);
                    for (auto &pair : userToMemory)
                        SHOWLN(pair.getFirst());
                }
            } else {
                maxNode = searchUsesForStores(user, nodeIndex, maxNode);
            }
        }
        return maxNode;
    }
    void connect(unsigned inIndex, unsigned outIndex) {
        nodes[inIndex].outNeighbors.insert(outIndex);
        nodes[outIndex].inNeighbors.insert(inIndex);
    }
    void connect(const Dependence &e) {
        if (e.in->nodeIndex != e.out->nodeIndex)
            connect(e.in->nodeIndex, e.out->nodeIndex);
    }
    void connectGraph() {

        // llvm::SmallVector<unsigned> map(memory.size(),
        // std::numeric_limits<unsigned>::max());
        // pair is <memory index, node index>
        // llvm::DenseMap<MemoryAccess *, unsigned> memAccessPtrToIndex;
        // memoryToNodeMap.resize(memory.size(),
        //                        std::numeric_limits<unsigned>::max());
        // assembles direct connections in node graph
        unsigned j = 0;
        for (unsigned i = 0; i < memory.size(); ++i) {
            MemoryAccess &mai = memory[i];
            if (mai.nodeIndex == std::numeric_limits<unsigned>::max())
                mai.nodeIndex = j++;
            if (mai.isLoad)
                // if load, we search all uses, looking for stores
                // we search loads, because that probably has a smaller set
                // (i.e., we only search users, avoiding things like
                // constants)
                j = searchUsesForStores(mai.user, mai.nodeIndex, j);
        }
        nodes.resize(j);
        for (unsigned i = 0; i < memory.size(); ++i) {
            MemoryAccess &mem = memory[i];
            mem.index = i;
            // for (auto &&mem : memory){
            ScheduledNode &node = nodes[mem.nodeIndex];
            node.memory.insert(i);
            node.numLoops = std::max(node.numLoops, uint8_t(mem.getNumLoops()));
        }
        for (auto &e : edges)
            connect(e.in->nodeIndex, e.out->nodeIndex);
        for (auto &&node : nodes)
            node.schedule.init(node.getNumLoops());
#ifndef NDEBUG
        for (auto &mem : memory) {
            SHOW(mem.nodeIndex);
            CSHOWLN(mem.ref);
        }
#endif
        // now that we've assigned each MemoryAccess to a NodeIndex, we
        // build the actual graph
    }
    struct Graph {
        // a subset of Nodes
        BitSet nodeIds;
        BitSet activeEdges;
        llvm::MutableArrayRef<MemoryAccess> mem;
        llvm::MutableArrayRef<ScheduledNode> nodes;
        llvm::ArrayRef<Dependence> edges;
        // llvm::SmallVector<bool> visited;
        // BitSet visited;
        Graph operator&(const Graph &g) {
            return Graph{nodeIds & g.nodeIds, activeEdges & g.activeEdges, mem,
                         nodes, edges};
        }
        Graph operator|(const Graph &g) {
            return Graph{nodeIds | g.nodeIds, activeEdges | g.activeEdges, mem,
                         nodes, edges};
        }
        Graph &operator&=(const Graph &g) {
            nodeIds &= g.nodeIds;
            activeEdges &= g.activeEdges;
            return *this;
        }
        Graph &operator|=(const Graph &g) {
            nodeIds |= g.nodeIds;
            activeEdges |= g.activeEdges;
            return *this;
        }
        [[nodiscard]] BitSet &inNeighbors(size_t i) {
            return nodes[i].inNeighbors;
        }
        [[nodiscard]] BitSet &outNeighbors(size_t i) {
            return nodes[i].outNeighbors;
        }
        [[nodiscard]] const BitSet &inNeighbors(size_t i) const {
            return nodes[i].inNeighbors;
        }
        [[nodiscard]] const BitSet &outNeighbors(size_t i) const {
            return nodes[i].outNeighbors;
        }
        [[nodiscard]] bool containsNode(size_t i) const {
            return nodeIds.contains(i);
        }
        [[nodiscard]] bool missingNode(size_t i) const {
            return !containsNode(i);
        }
        [[nodiscard]] bool missingNode(size_t i, size_t j) const {
            return !(containsNode(i) && containsNode(j));
        }
        [[nodiscard]] bool missingNode(const Dependence &e) const {
            return missingNode(e.in->nodeIndex, e.out->nodeIndex);
        }

        [[nodiscard]] bool isInactive(const Dependence &edge, size_t d) const {
            return edge.isInactive(d) || missingNode(edge);
        }
        [[nodiscard]] bool isInactive(const Dependence &edge) const {
            return missingNode(edge);
        }
        [[nodiscard]] bool isInactive(size_t e, size_t d) const {
            return !(activeEdges[e]) || isInactive(edges[e], d);
        }
        [[nodiscard]] bool isInactive(size_t e) const {
            return !(activeEdges[e]) || isInactive(edges[e]);
        }
        [[nodiscard]] bool isActive(size_t e, size_t d) const {
            return (activeEdges[e]) && (!isInactive(edges[e], d));
        }
        [[nodiscard]] bool isActive(size_t e) const {
            return (activeEdges[e]) && (!isInactive(edges[e]));
        }
        BitSliceView<ScheduledNode>::Iterator begin() {
            return BitSliceView<ScheduledNode>{nodes, nodeIds}.begin();
        }
        BitSliceView<ScheduledNode>::ConstIterator begin() const {
            const BitSliceView<ScheduledNode> bsv{nodes, nodeIds};
            return bsv.begin();
        }
        BitSet::Iterator::End end() const { return {}; }
        bool wasVisited(size_t i) const { return nodes[i].visited; }
        void visit(size_t i) { nodes[i].visit(); }
        void unVisit(size_t i) { nodes[i].unVisit(); }
        size_t getNumVertices() const { return nodeIds.size(); }
        size_t maxVertexId() const { return nodeIds.maxValue(); }
        BitSet &vertexIds() { return nodeIds; }
        const BitSet &vertexIds() const { return nodeIds; }
        Graph subGraph(const BitSet &components) {
            return {components, activeEdges, mem, nodes, edges};
        }
        llvm::SmallVector<Graph, 0>
        split(const llvm::SmallVector<BitSet> &components) {
            llvm::SmallVector<Graph, 0> graphs;
            graphs.reserve(components.size());
            for (auto &c : components)
                graphs.push_back(subGraph(c));
            return graphs;
        }
        [[nodiscard]] size_t calcMaxDepth() const {
            size_t d = 0;
            for (auto n : nodeIds)
                d = std::max(d, nodes[n].getNumLoops());
            return d;
        }
    };
    // bool connects(const Dependence &e, Graph &g0, Graph &g1, size_t d) const
    // {
    //     return ((e.in->getNumLoops() > d) && (e.out->getNumLoops() > d)) &&
    //            connects(e, g0, g1);
    // }
    bool connects(const Dependence &e, Graph &g0, Graph &g1) const {
        size_t nodeIn = e.in->nodeIndex;
        size_t nodeOut = e.out->nodeIndex;
        return ((g0.nodeIds.contains(nodeIn) && g1.nodeIds.contains(nodeOut)) ||
                (g0.nodeIds.contains(nodeOut) && g1.nodeIds.contains(nodeIn)));
    }
    Graph fullGraph() {
        return {BitSet::dense(nodes.size()), BitSet::dense(edges.size()),
                memory, nodes, edges};
    }
    void fillUserToMemoryMap() {
        for (auto &mem : memory)
            userToMemory.insert(std::make_pair(mem.user, &mem));
    }
    // TODO: we need to rotate via setting the schedule, not instantiating
    // the rotated array!
    [[nodiscard]] static llvm::IntrusiveRefCntPtr<AffineLoopNest>
    getBang(llvm::DenseMap<const AffineLoopNest *,
                           llvm::IntrusiveRefCntPtr<AffineLoopNest>> &map,
            SquarePtrMatrix<int64_t> K, const AffineLoopNest *aln) {
        auto p = map.find(aln);
        llvm::IntrusiveRefCntPtr<AffineLoopNest> newp;
        if (p != map.end()) {
            return p->second;
        } else {
            const size_t numVar = aln->getNumLoops();
            const size_t numTransformed = K.numCol();
            const size_t numPeeled = numVar - numTransformed;
            // A = aln->A*K';
            // IntMatrix A(IntMatrix::uninitialized(numConstraints,
            // numVar)); for (size_t j = 0; j < numPeeled; ++j) {
            //     for (size_t k = 0; k < numConstraints; ++k) {
            //         A(k, j) = aln->A(k, j);
            //     }
            // }
            // for (size_t j = numPeeled; j < numVar; ++j) {
            //     for (size_t k = 0; k < numConstraints; ++k) {
            //         int64_t Akj = 0;
            //         for (size_t l = 0; l < numTransformed; ++l) {
            //             Akj += aln->A(k, l) * K(j - numPeeled, l);
            //         }
            //         A(k, j) = Akj;
            //     }
            // }
            auto alshr = aln->rotate(K, numPeeled);
            // auto alshr = llvm::makeIntrusiveRefCnt<AffineLoopNest>(
            //     std::move(A), aln->b, aln->poset);
            map.insert(std::make_pair(aln, alshr));
            return alshr;
        }
    }
    //    bool ilpOpt() { return false; }

    void orthogonalizeStoresOld() {
        llvm::SmallVector<bool, 256> visited(memory.size());
        for (size_t i = 0; i < memory.size(); ++i) {
            if (visited[i])
                continue;
            MemoryAccess &mai = memory[i];
            if (mai.isLoad)
                continue;
            visited[i] = true;
            ArrayReference &refI = mai.ref;
            size_t dimI = refI.arrayDim();
            auto indMatI = refI.indexMatrix();
            size_t numLoopsI = indMatI.numRow();
            size_t multiInds = 0;
            size_t multiLoops = 0;
            for (size_t j = 0; j < dimI; ++j) {
                size_t count = 0;
                size_t loopsJ = 0;
                for (size_t k = 0; k < numLoopsI; ++k) {
                    if (indMatI(k, j)) {
                        ++count;
                        loopsJ |= (size_t(1) << k);
                    }
                }
                if (count > 1) {
                    multiLoops |= loopsJ;
                    multiInds |= (size_t(1) << j);
                }
            }
            if (multiInds == 0)
                continue;
            // we won't rotate peelOuter loops
            const size_t peelOuter = std::countr_zero(multiLoops);
            size_t numLoops = refI.getNumLoops();
            // size_t numLoad = 0;
            size_t numStore = 1;
            size_t numRow = refI.arrayDim();
            // we prioritize orthogonalizing stores
            // therefore, we sort the loads after
            llvm::SmallVector<unsigned, 16> orthInds;
            orthInds.push_back(i);
            for (size_t j = 0; j < memory.size(); ++j) {
                if (i == j)
                    continue;
                MemoryAccess &maj = memory[j];
                if (!mai.fusedThrough(maj))
                    continue;
                ArrayReference &refJ = maj.ref;
                numLoops = std::max(numLoops, refJ.getNumLoops());
                numRow += refJ.arrayDim();
                // numLoad += maj.isLoad;
                numStore += (!maj.isLoad);
                // TODO: maybe don't set so aggressive, e.g.
                // if orth fails we could still viably set a narrower subset
                // or if it succeeds, perhaps a wider one.
                // So the item here is to adjust peelOuter.
                orthInds.push_back(j);
            }
            IntMatrix S(numLoops - peelOuter, numRow);
            size_t rowStore = 0;
            size_t rowLoad = numStore;
            bool dobreakj = false;
            for (auto j : orthInds) {
                MemoryAccess &maj = memory[j];
                ArrayReference &refJ = maj.ref;
                size_t row = maj.isLoad ? rowLoad : rowStore;
                auto indMatJ = refJ.indexMatrix();
                for (size_t l = peelOuter; l < indMatJ.numRow(); ++l) {
                    for (size_t k = 0; k < indMatJ.numCol(); ++k) {
                        S(l - peelOuter, row + k) = indMatJ(l, k);
                    }
                }
                row += indMatJ.numCol();
                if (dobreakj)
                    break;
                rowLoad = maj.isLoad ? row : rowLoad;
                rowStore = maj.isLoad ? rowStore : row;
            }
            if (dobreakj)
                continue;
            auto [K, included] = NormalForm::orthogonalize(S);
            if (included.size()) {
                // L = old inds, J = new inds
                // L = K*J
                // Bounds:
                // A*L <= b
                // (A*J)*J <= b
                // Indices:
                // S*L = (S*K)*J
                // Schedule:
                // Phi*L = (Phi*K)*J
                IntMatrix KS{K * S};
                llvm::DenseMap<const AffineLoopNest *,
                               llvm::IntrusiveRefCntPtr<AffineLoopNest>>
                    loopMap;
                rowStore = 0;
                rowLoad = numStore;
                for (unsigned j : orthInds) {
                    visited[j] = true;
                    MemoryAccess &maj = memory[j];
                    // unsigned oldRefID = maj.ref;
                    // if (refMap[oldRefID] >= 0) {
                    //     maj.ref = oldRefID;
                    //     continue;
                    // }
                    ArrayReference oldRef = std::move(maj.ref);
                    maj.ref = {oldRef.arrayID,
                               getBang(loopMap, K, oldRef.loop.get())};
                    // refMap[oldRefID] = maj.ref = refs.size();
                    // refs.emplace_back(
                    size_t row = maj.isLoad ? rowLoad : rowStore;
                    auto indMatJ = oldRef.indexMatrix();
                    for (size_t l = peelOuter; l < indMatJ.numRow(); ++l)
                        for (size_t k = 0; k < indMatJ.numCol(); ++k)
                            indMatJ(l, k) = KS(l - peelOuter, row + k);
                    row += indMatJ.numCol();
                    rowLoad = maj.isLoad ? row : rowLoad;
                    rowStore = maj.isLoad ? rowStore : row;
                    // set maj's schedule to rotation
                    // phi * L = (phi * K) * J
                    // NOTE: we're assuming the schedule is the identity
                    // otherwise, new schedule = old schedule * K
                    MutSquarePtrMatrix<int64_t> Phi = maj.schedule.getPhi();
                    // size_t phiDim = Phi.numCol();
                    Phi = K(_(peelOuter, end), _(peelOuter, end));
                }
            }
        }
    }
    // NOTE: requires omniSimplex to be instantiated
    bool orthogonalizeStores() {
        // as a first optimization, we just look for stores with lower
        // access rank than the associated loop nest depth
        // Note that we can handle loads later, e.g. once we
        // hit the rank (and things have been feasible)
        // we can try adding extra constraints to
        // orthogonalize loads with respect to stores
        //
        // stores id, rank pairs
        llvm::SmallVector<unsigned> orthCandidates;
        scheduled.clear();
        scheduled.resize(memory.size()); // should be all `false`
        // iterate over stores
        for (unsigned i = 0; i < unsigned(memory.size()); ++i) {
            MemoryAccess &mai = memory[i];
            if (mai.isLoad)
                continue;
            mai.ref.rank = NormalForm::rank(mai.indexMatrix());
            if (mai.ref.rank < mai.getNumLoops())
                orthCandidates.emplace_back(i);
        }
        if (orthCandidates.size() == 0)
            return false;
        // now, the approach for orthogonalizing is to
        // first try orthogonalizing all, then, if that fails
        // we (as a first implementation) don't orthogonalize.
        // in the future, we could try stepwise, perhaps
        // using some cost model to prioritize.
        for (auto i : orthCandidates) {
            MemoryAccess &mem = memory[i];
            // now we set the schedule to the memory accesses
            auto indMat = mem.indexMatrix(); // numLoops x arrayDim
            auto phi = mem.schedule.getPhi();
            // we flip order, to try and place higher stride
            // dimensions in the outer loops
            for (size_t r = 0; r < mem.ref.rank; ++r)
                phi(r, _) = indMat(_, mem.ref.rank - 1 - r);
        }
        // TODO: probably have this be a separate driver instead?
        return false;
    }

    [[nodiscard]] size_t countNumLambdas(const Graph &g, size_t d) const {
        size_t c = 0;
        for (size_t e = 0; e < edges.size(); ++e)
            c += ((g.isInactive(e, d)) ? 0 : edges[e].getNumLambda());
        return c;
    }
    [[nodiscard]] size_t countNumBoundingCoefs(const Graph &g, size_t d) const {
        size_t c = 0;
        for (size_t e = 0; e < edges.size(); ++e)
            c += (g.isInactive(e, d) ? 0 : edges[e].getNumSymbols());
        return c;
    }
    void countAuxParamsAndConstraints(const Graph &g, size_t d) {
        size_t a = 0, b = 0, c = 0, ae = 0;
        for (size_t e = 0; e < edges.size(); ++e) {
            if (g.isInactive(e, d))
                continue;
            const Dependence &edge = edges[e];
            a += edge.getNumLambda();
            b += edge.depPoly.symbols.size();
            c += edge.getNumConstraints();
            ++ae;
        }
        numLambda = a;
        numBounding = b;
        numConstraints = c;
        numActiveEdges = ae;
    }
    void countNumParams(const Graph &g, size_t depth) {
        setScheduleMemoryOffsets(g, depth);
        countAuxParamsAndConstraints(g, depth);
    }
    // assemble omni-simplex
    // we want to order variables to be
    // us, ws, Phi^-, Phi^+, omega, lambdas
    // this gives priority for minimization

    // bounding, scheduled coefs, lambda
    // matches lexicographical ordering of minimization
    // bounding, however, is to be favoring minimizing `u` over `w`
    [[nodiscard]] size_t getLambdaOffset() const {
        return 1 + numBounding + numActiveEdges + numPhiCoefs + numOmegaCoefs;
    }
    [[nodiscard]] bool hasActiveEdges(const Graph &g,
                                      const MemoryAccess &mem) const {
        for (auto &e : mem.edgesIn)
            if (!g.isInactive(e))
                return true;
        for (auto &e : mem.edgesOut)
            if (!g.isInactive(e))
                return true;
        return false;
    }
    [[nodiscard]] bool hasActiveEdges(const Graph &g, const MemoryAccess &mem,
                                      size_t d) const {
        for (auto &e : mem.edgesIn)
            if (!g.isInactive(e, d))
                return true;
        for (auto &e : mem.edgesOut)
            if (!g.isInactive(e, d))
                return true;
        return false;
    }
    [[nodiscard]] bool hasActiveEdges(const Graph &g, const ScheduledNode &node,
                                      size_t d) const {
        for (auto memId : node.memory)
            if (hasActiveEdges(g, memory[memId], d))
                return true;
        return false;
    }
    [[nodiscard]] bool hasActiveEdges(const Graph &g,
                                      const ScheduledNode &node) const {
        for (auto memId : node.memory)
            if (hasActiveEdges(g, memory[memId]))
                return true;
        return false;
    }
    void setScheduleMemoryOffsets(const Graph &g, size_t d) {
        size_t pInit = numBounding + numActiveEdges + 1, p = pInit;
        for (auto &&node : nodes) {
            SHOW(d);
            CSHOW(node.getNumLoops());
            CSHOWLN(hasActiveEdges(g, node, d));
            if ((d >= node.getNumLoops()) || (!hasActiveEdges(g, node, d)))
                continue;
            if (!node.phiIsScheduled())
                p = node.updatePhiOffset(p);
        }
        numPhiCoefs = p - pInit;
        std::cout << "\njust set schedule mem offsets";
        CSHOWLN(numPhiCoefs);
        size_t o = p;
        for (auto &&node : nodes) {
            if ((d > node.getNumLoops()) || (!hasActiveEdges(g, node, d)))
                continue;
            o = node.updateOmegaOffset(o);
        }
        numOmegaCoefs = o - p;
    }
    void validateMemory() {
        for (auto &mem : memory)
            assert(mem.ref.getNumLoops() == mem.schedule.getNumLoops());
    }
    void validateEdges() {
        for (auto &edge : edges) {
            SHOWLN(edge);
            assert(edge.in->getNumLoops() + edge.out->getNumLoops() ==
                   edge.getNumPhiCoefficients());
            assert(1 + edge.depPoly.getNumLambda() +
                       edge.getNumPhiCoefficients() +
                       edge.getNumOmegaCoefficients() ==
                   edge.dependenceSatisfaction.getConstraints().numCol());
        }
    }
    void instantiateOmniSimplex(const Graph &g, size_t d) {
        // defines numScheduleCoefs, numLambda, numBounding, and
        // numConstraints
        omniSimplex.reserve(numConstraints + numOmegaCoefs,
                            1 + numBounding + numActiveEdges + numPhiCoefs +
                                2 * numOmegaCoefs + numLambda);
        omniSimplex.resizeForOverwrite(
            numConstraints, 1 + numBounding + numActiveEdges + numPhiCoefs +
                                numOmegaCoefs + numLambda);
        auto C{omniSimplex.getConstraints()};
        C = 0;
        // layout of omniSimplex:
        // Order: C, then priority to minimize
        // all : C, u, w, Phis, omegas, lambdas
        // rows give constraints; each edge gets its own
        // constexpr size_t numOmega =
        //     DependencePolyhedra::getNumOmegaCoefficients();
        size_t u = 1, w = 1 + numBounding;
        size_t c = 0, l = getLambdaOffset();
#ifndef NDEBUG
        size_t minPhiCoefInd = std::numeric_limits<size_t>::max();
        size_t minOmegaCoefInd = std::numeric_limits<size_t>::max();
        size_t maxPhiCoefInd = 0;
        size_t maxOmegaCoefInd = 0;
        // size_t numActiveCount = 0;
#endif
        for (size_t e = 0; e < edges.size(); ++e) {
            Dependence &edge = edges[e];
            // #ifndef NDEBUG
            //             std::cout << ";edge=" << e;
            // #endif
            if (g.isInactive(e, d))
                continue;
            // #ifndef NDEBUG
            //             std::cout << "; is active!";
            //             ++numActiveCount;
            // #endif
            unsigned outNodeIndex = edge.out->nodeIndex;
            unsigned inNodeIndex = edge.in->nodeIndex;
            // SHOW(outNodeIndex);
            // CSHOWLN(inNodeIndex);
            // if (outNodeIndex == inNodeIndex ||
            //     g.missingNode(outNodeIndex, inNodeIndex))
            //     continue;
            // std::cout<<"; is still active!";
            const ScheduledNode &outNode = nodes[outNodeIndex];
            const ScheduledNode &inNode = nodes[inNodeIndex];
            const auto [satC, satL, satPp, satPc, satO] =
                edge.splitSatisfaction();
            const auto [bndC, bndL, bndPp, bndPc, bndO, bndWU] =
                edge.splitBounding();

            const size_t numSatConstraints = satC.size();
            const size_t numBndConstraints = bndC.size();
            size_t cc = c + numSatConstraints;
            size_t ccc = cc + numBndConstraints;

            size_t ll = l + satL.numCol();
            size_t lll = ll + bndL.numCol();
#ifndef NDEBUG
            SHOW(C.numRow());
            CSHOW(C.numCol());
            CSHOW(c);
            CSHOW(cc);
            CSHOW(l);
            CSHOW(ll);
            CSHOWLN(lll);
#endif
            C(_(c, cc), _(l, ll)) = satL;
            C(_(cc, ccc), _(ll, lll)) = bndL;
            l = lll;

            // bounding
            C(_(cc, ccc), w++) = bndWU(_, 0);
            size_t uu = u + bndWU.numCol() - 1;
            C(_(cc, ccc), _(u, uu)) = bndWU(_, _(1, end));
            u = uu;

            C(_(c, cc), 0) = satC;
            C(_(cc, ccc), 0) = bndC;
            // now, handle Phi and Omega
            // phis are not constrained to be 0
            if (outNodeIndex == inNodeIndex) {
                if (d < outNode.getNumLoops()) {
                    if (satPc.numCol() == satPp.numCol()) {
                        if (outNode.phiIsScheduled()) {
                            // add it constants
                            auto sch = edge.out->getSchedule(d);
                            C(_(c, cc), 0) -= satPc * sch + satPp * sch;
                            C(_(cc, ccc), 0) -= bndPc * sch + bndPp * sch;
                        } else {
                            auto phiChild = outNode.getPhiOffset();
                            C(_(c, cc), phiChild) = satPc + satPp;
                            C(_(cc, ccc), phiChild) = bndPc + bndPp;
                        }
                    } else {
                        if (outNode.phiIsScheduled()) {
                            // add it constants
                            // note that loop order in schedule goes
                            // inner -> outer
                            // so we need to drop inner most if one has less
                            auto sch = edge.out->getSchedule(d);
                            auto schP = sch(_(end - satPp.numCol(), end));
                            auto schC = sch(_(end - satPc.numCol(), end));
                            C(_(c, cc), 0) -= satPc * schC + satPp * schP;
                            C(_(cc, ccc), 0) -= bndPc * schC + bndPp * schP;
                        } else if (satPc.numCol() < satPp.numCol()) {
                            auto phiChild = outNode.getPhiOffset();
                            size_t P = satPc.numCol();
                            auto m = phiChild.e - P;
                            C(_(c, cc), _(m, phiChild.e)) =
                                satPc + satPp(_, _(end - P, end));
                            C(_(cc, ccc), _(m, phiChild.e)) =
                                bndPc + bndPp(_, _(end - P, end));
                            C(_(c, cc), _(phiChild.b, m)) = satPp;
                            C(_(cc, ccc), _(phiChild.b, m)) = bndPp;
                        } else /* if (satPc.numCol() > satPp.numCol()) */ {
                            auto phiChild = outNode.getPhiOffset();
                            size_t P = satPp.numCol();
                            auto m = phiChild.e - P;
                            C(_(c, cc), _(m, phiChild.e)) =
                                satPc(_, _(end - P, end)) + satPp;
                            C(_(cc, ccc), _(m, phiChild.e)) =
                                bndPc(_, _(end - P, end)) + bndPp;
                            C(_(c, cc), _(phiChild.b, m)) = satPc;
                            C(_(cc, ccc), _(phiChild.b, m)) = bndPc;
                        }
                    }
                    C(_(c, cc), outNode.omegaOffset) = satO(_, 0) + satO(_, 1);
                    C(_(cc, ccc), outNode.omegaOffset) =
                        bndO(_, 0) + bndO(_, 1);
                }
            } else {
                if (d >= edge.out->getNumLoops()) {
                } else if (outNode.phiIsScheduled()) {
                    // add it constants
                    auto sch = edge.out->getSchedule(d);
                    SHOWLN(edge.out->getNumLoops());
                    SHOW(satPc.numRow());
                    CSHOW(satPc.numCol());
                    CSHOWLN(sch.size());
                    C(_(c, cc), 0) -= satPc * sch;
                    C(_(cc, ccc), 0) -= bndPc * sch;
                } else {
                    assert(satPc.numCol() == bndPc.numCol());
                    // add it to C
                    auto phiChild = outNode.getPhiOffset();
                    C(_(c, cc), phiChild) = satPc;
                    // C(_(c, cc), phiChild + satPc.numCol()) = satPc;
                    C(_(cc, ccc), phiChild) = bndPc;
                    // C(_(cc, ccc), phiChild + bndPc.numCol()) = bndPc;
#ifndef NDEBUG
                    minPhiCoefInd = std::min(minPhiCoefInd, phiChild.b);
                    maxPhiCoefInd = std::max(maxPhiCoefInd, phiChild.e);
#endif
                }
                if (d >= edge.in->getNumLoops()) {
                } else if (inNode.phiIsScheduled()) {
                    // add it to constants
                    auto sch = edge.in->getSchedule(d);
                    SHOWLN(edge.in->getNumLoops());
                    SHOW(satPp.numRow());
                    CSHOW(satPp.numCol());
                    CSHOWLN(sch.size());
                    C(_(c, cc), 0) -= satPp * sch;
                    C(_(cc, ccc), 0) -= bndPp * sch;
                } else {
                    assert(satPp.numCol() == bndPp.numCol());
                    // add it to C
                    auto phiParent = inNode.getPhiOffset();
                    C(_(c, cc), phiParent) = satPp;
                    // C(_(c, cc), phiParent + satPp.numCol()) = satPp;
                    C(_(cc, ccc), phiParent) = bndPp;
                    // C(_(cc, ccc), phiParent + bndPp.numCol()) = bndPp;
#ifndef NDEBUG
                    minPhiCoefInd = std::min(minPhiCoefInd, phiParent.b);
                    maxPhiCoefInd = std::max(maxPhiCoefInd, phiParent.e);
#endif
                }
                // Omegas are included regardless of rotation
                if (d < edge.out->getNumLoops()) {
                    C(_(c, cc), outNode.omegaOffset) = satO(_, !edge.forward);
                    C(_(cc, ccc), outNode.omegaOffset) = bndO(_, !edge.forward);
#ifndef NDEBUG
                    minOmegaCoefInd =
                        std::min(minOmegaCoefInd, size_t(outNode.omegaOffset));
                    maxOmegaCoefInd =
                        std::max(maxOmegaCoefInd, size_t(outNode.omegaOffset));
#endif
                }
                if (d < edge.in->getNumLoops()) {
                    C(_(c, cc), inNode.omegaOffset) = satO(_, edge.forward);
                    C(_(cc, ccc), inNode.omegaOffset) = bndO(_, edge.forward);
#ifndef NDEBUG
                    minOmegaCoefInd =
                        std::min(minOmegaCoefInd, size_t(inNode.omegaOffset));
                    maxOmegaCoefInd =
                        std::max(maxOmegaCoefInd, size_t(inNode.omegaOffset));
#endif
                }
            }
            c = ccc;
        }
        // #ifndef NDEBUG
        //         std::cout << std::endl;
        //         // SHOWLN(numActiveCount);
        //         SHOW(1 + numBounding + numActiveEdges + numPhiCoefs);
        //         CSHOWLN(minOmegaCoefInd);
        //         SHOW(numBounding + numActiveEdges + numPhiCoefs +
        //         numOmegaCoefs); CSHOWLN(maxOmegaCoefInd); SHOW(d);
        //         CSHOW(numBounding);
        //         CSHOW(numActiveEdges);
        //         CSHOW(numPhiCoefs);
        //         CSHOW(numOmegaCoefs);
        //         CSHOWLN(numLambda);
        //         assert(minOmegaCoefInd >=
        //                1 + numBounding + numActiveEdges + numPhiCoefs);
        //         assert(maxOmegaCoefInd <=
        //                numBounding + numActiveEdges + numPhiCoefs +
        //                numOmegaCoefs);
        //         assert(minPhiCoefInd >= 1 + numBounding + numActiveEdges);
        //         assert(maxPhiCoefInd <= 1 + numBounding + numActiveEdges +
        //         numPhiCoefs); SHOWLN(C); assert(!allZero(C(_, end))); size_t
        //         nonZeroC = 0; for (size_t j = 0; j < C.numRow(); ++j)
        //             for (size_t i = 0; i < C.numCol(); ++i)
        //                 nonZeroC += (C(j, i) != 0);
        //         size_t nonZeroEdges = 0;
        //         for (size_t eid = 0; eid < edges.size(); ++eid) {
        //             const Dependence &e = edges[eid];
        //             if (g.isInactive(eid, d))
        //                 continue;
        //             auto edb = e.dependenceBounding.getConstraints();
        //             bool firstEdgeActive = d < e.out->getNumLoops();
        //             bool secondEdgeActive = d < e.in->getNumLoops();
        //             size_t nodeIn = e.in->nodeIndex;
        //             size_t nodeOut = e.out->nodeIndex;
        //             // forward means [in, out], !forward means [out, in]
        //             if (e.forward)
        //                 std::swap(firstEdgeActive, secondEdgeActive);
        //             for (size_t j = 0; j < edb.numRow(); ++j) {
        //                 for (size_t i = 0; i < 1 + e.depPoly.getNumLambda();
        //                 ++i)
        //                     nonZeroEdges += (edb(j, i) != 0);
        //                 size_t bound = 1 + e.depPoly.getNumLambda();
        //                 size_t ub0 = bound + e.depPoly.getDim0();
        //                 if (nodeIn == nodeOut) {
        //                     if (firstEdgeActive || secondEdgeActive) {
        //                         size_t ub1 =
        //                         e.depPoly.getNumPhiCoefficients(); for
        //                         (size_t i = bound; i < ub0; ++i)
        //                             nonZeroEdges +=
        //                                 ((edb(j, i) + edb(j, i + ub1)) != 0);
        //                         bound += e.getNumPhiCoefficients();
        //                         nonZeroEdges +=
        //                             ((edb(j, bound) + edb(j, bound + 1)) !=
        //                             0);
        //                     }
        //                 } else {
        //                     if (firstEdgeActive)
        //                         for (size_t i = bound; i < ub0; ++i)
        //                             nonZeroEdges += (edb(j, i) != 0);
        //                     size_t ub1 = bound +
        //                     e.depPoly.getNumPhiCoefficients(); if
        //                     (secondEdgeActive)
        //                         for (size_t i = ub0; i < ub1; ++i)
        //                             nonZeroEdges += (edb(j, i) != 0);

        //                     bound += e.getNumPhiCoefficients();
        //                     if (firstEdgeActive)
        //                         nonZeroEdges += (edb(j, bound) != 0);
        //                     if (secondEdgeActive)
        //                         nonZeroEdges += (edb(j, bound + 1) != 0);
        //                 }
        //                 for (size_t i = bound + 2; i < edb.numCol(); ++i)
        //                     nonZeroEdges += (edb(j, i) != 0);
        //             }
        //             auto eds = e.dependenceSatisfaction.getConstraints();
        //             for (size_t j = 0; j < eds.numRow(); ++j) {
        //                 for (size_t i = 0; i < 1 + e.depPoly.getNumLambda();
        //                 ++i)
        //                     nonZeroEdges += (eds(j, i) != 0);
        //                 size_t bound = 1 + e.depPoly.getNumLambda();
        //                 size_t ub0 = bound + e.depPoly.getDim0();
        //                 if (nodeIn == nodeOut) {
        //                     if (firstEdgeActive || secondEdgeActive) {
        //                         size_t ub1 =
        //                         e.depPoly.getNumPhiCoefficients(); for
        //                         (size_t i = bound; i < ub0; ++i)
        //                             nonZeroEdges +=
        //                                 ((eds(j, i) + eds(j, i + ub1)) != 0);
        //                         bound += e.getNumPhiCoefficients();
        //                         nonZeroEdges +=
        //                             ((eds(j, bound) + eds(j, bound + 1)) !=
        //                             0);
        //                     }
        //                 } else {
        //                     if (firstEdgeActive)
        //                         for (size_t i = bound; i < ub0; ++i)
        //                             nonZeroEdges += (eds(j, i) != 0);
        //                     size_t ub1 = bound +
        //                     e.depPoly.getNumPhiCoefficients(); if
        //                     (secondEdgeActive)
        //                         for (size_t i = ub0; i < ub1; ++i)
        //                             nonZeroEdges += (eds(j, i) != 0);
        //                     bound += e.getNumPhiCoefficients();
        //                     if (firstEdgeActive)
        //                         nonZeroEdges += (eds(j, bound) != 0);
        //                     if (secondEdgeActive)
        //                         nonZeroEdges += (eds(j, bound + 1) != 0);
        //                 }
        //                 for (size_t i = bound + 2; i < eds.numCol(); ++i)
        //                     nonZeroEdges += (eds(j, i) != 0);
        //             }
        //         }
        //         SHOW(nonZeroC);
        //         CSHOWLN(nonZeroEdges);
        //         assert(nonZeroC == nonZeroEdges);

        // #endif
    }
    void deactivateSatisfiedEdges(Graph &g, PtrVector<Rational> sol,
                                  size_t d) const {
        if (allZero(sol(_(begin, numBounding + numActiveEdges))))
            return;
        size_t u = 0, w = numBounding;
        SHOWLN(sol);
        for (size_t e = 0; e < edges.size(); ++e) {
            if (g.isInactive(e, d))
                continue;
            const Dependence &edge = edges[e];
            size_t uu =
                u + edge.dependenceBounding.getConstraints().numCol() -
                (2 + edge.depPoly.getNumLambda() +
                 edge.getNumPhiCoefficients() + edge.getNumOmegaCoefficients());
            SHOW(sol.size());
            CSHOW(w);
            CSHOW(u);
            CSHOWLN(uu);
            SHOW(sol(w));
            std::cout << "; ";
            CSHOWLN(sol(_(u, uu)));
            if (sol(w++) || (!(allZero(sol(_(u, uu)))))) {
                std::cout << "Removing edge = " << e << std::endl;
                g.activeEdges.remove(e);
            }
            u = uu;
        }
    }
    void updateSchedules(const Graph &g, PtrVector<Rational> sol,
                         size_t depth) {
        SHOW(depth);
        CSHOWLN(sol);
#ifndef NDEBUG
        if (depth & 1) {
            bool allZero = true;
            for (auto &s : sol) {
                allZero &= (s == 0);
            }
            if (allZero)
                SHOWLN(omniSimplex);
            assert(!allZero);
        }
#endif
        for (auto &&node : nodes) {
            if (depth >= node.getNumLoops())
                continue;
            if (!hasActiveEdges(g, node)) {
                // std::cout << "No active edges!!!!!!"<<std::endl;
                node.schedule.getOmega()(2 * depth + 1) =
                    std::numeric_limits<int64_t>::min();
                if (!node.phiIsScheduled())
                    node.schedule.getPhi()(depth, _) =
                        std::numeric_limits<int64_t>::min();
                continue;
            }
            node.schedule.getOmega()(2 * depth + 1) = sol(node.omegaOffset - 1);
            if (!node.phiIsScheduled())
                SHOWLN(sol(node.getPhiOffset() - 1));
            if (!node.phiIsScheduled())
                node.schedule.getPhi()(depth, _) = sol(node.getPhiOffset() - 1);
            SHOW(depth);
            CSHOWLN(node.schedule.getPhi()(depth, _));
        }
    }
    [[nodiscard]] static int64_t lexSign(PtrVector<int64_t> x) {
        for (auto y : x)
            if (y)
                return 2 * (y > 0) - 1;
        return 0;
    }
    void addIndependentSolutionConstraints(const Graph &g, size_t depth) {
        std::cout << "addIndependentSolutionConstraints(depth = " << depth
                  << ")" << std::endl;
        omniSimplex.reserveExtraRows(memory.size());
        if (depth == 0) {
            // add ones >= 0
            for (auto &&node : nodes) {
                if (node.phiIsScheduled() || (!hasActiveEdges(g, node)))
                    continue;
                auto c{omniSimplex.addConstraintAndVar()};
                c(0) = 1;
                c(node.getPhiOffset()) = 1;
                c(end) = -1; // for >=
            }
            return;
        }
        IntMatrix A, N;
        for (auto &&node : nodes) {
            if (node.phiIsScheduled() || (depth >= node.getNumLoops()) ||
                (!hasActiveEdges(g, node)))
                continue;
            A = node.schedule.getPhi()(_(0, depth), _).transpose();
            std::cout << "indep constraint; ";
            SHOW(depth);
            CSHOWLN(A);
            NormalForm::nullSpace11(N, A);
            SHOWLN(N);
            auto c{omniSimplex.addConstraintAndVar()};
            c(0) = 1;
            MutPtrVector<int64_t> cc{c(node.getPhiOffset())};
            // sum(N,dims=1) >= 1 after flipping row signs to be lex > 0
            for (size_t m = 0; m < N.numRow(); ++m)
                cc += N(m, _) * lexSign(N(m, _));
            SHOWLN(cc);
            c(end) = -1; // for >=
        }
    }
    void setSchedulesIndependent(const Graph &g, size_t depth) {
        IntMatrix A, N;
        for (auto &&node : nodes) {
            if ((depth >= node.getNumLoops()) || node.phiIsScheduled())
                continue;
            if (!hasActiveEdges(g, node)) {
                std::cout << "No active edges setIndep!!!!!!" << std::endl;
                node.schedule.getOmega()(2 * depth + 1) =
                    std::numeric_limits<int64_t>::min();
                if (!node.phiIsScheduled())
                    node.schedule.getPhi()(depth, _) =
                        std::numeric_limits<int64_t>::min();
                continue;
            }
            node.schedule.getOmega()(2 * depth + 1) = 0;
            MutSquarePtrMatrix<int64_t> phi = node.schedule.getPhi();
            if (depth) {
                A = phi(_(0, depth), _).transpose();
                NormalForm::nullSpace11(N, A);
                phi(depth, _) = N(0, _) * lexSign(N(0, _));
            } else {
                phi(depth, _(begin, end - 1)) = 0;
                phi(depth, end) = 1;
            }
        }
    }
    void resetPhiOffsets() {
        for (auto &&node : nodes)
            node.phiOffset = std::numeric_limits<unsigned>::max();
    }
    bool isSatisfied(Dependence &e, size_t d) {
        Schedule *first = &(nodes[e.in->nodeIndex].schedule);
        Schedule *second = &(nodes[e.out->nodeIndex].schedule);
        if (!e.forward)
            std::swap(first, second);
        return e.isSatisfied(*first, *second, d);
    }
    bool canFuse(Graph &g0, Graph &g1, size_t d) {
        for (auto &e : edges) {
            if ((e.in->getNumLoops() <= d) || (e.out->getNumLoops() <= d))
                return false;
            if (connects(e, g0, g1))
                if (!isSatisfied(e, d))
                    return false;
        }
        return true;
    }
    [[nodiscard]] bool breakGraph(Graph &g, Vector<Rational> &sol, size_t d) {
        auto components = Graphs::stronglyConnectedComponents(g);
        if (components.size() <= 1)
            return true;
        // components are sorted in topological order.
        // We split all of them, solve independently,
        // and then try to fuse again after if/where optimal schedules
        // allow it.
        auto graphs = g.split(components);
        assert(graphs.size() == components.size());
        for (auto &sg : graphs) {
            if (d >= sg.calcMaxDepth())
                continue;
            std::cout << "About to opt level:" << std::endl;
            countAuxParamsAndConstraints(sg, d);
            setScheduleMemoryOffsets(sg, d);
            if (optimizeLevel(sg, sol, d))
                return true; // give up
        }
        size_t unfusedOffset = 0;
        // For now, just greedily try and fuse from top down
        // we do this by setting the Omegas in a loop.
        // If fusion is legal, we don't increment the Omega offset.
        // else, we do.
        Graph *gp = &graphs[0];
        llvm::SmallVector<unsigned> baseGraphs;
        baseGraphs.push_back(0);
        for (size_t i = 1; i < components.size(); ++i) {
            Graph &gi = graphs[i];
            if (canFuse(*gp, gi, d)) {
                // fuse
                (*gp) |= gi;
            } else {
                // do not fuse
                for (auto &&v : *gp)
                    v.schedule.getOmega()[2 * d] = unfusedOffset;
                ++unfusedOffset;
                // gi is the new base graph
                gp = &gi;
                baseGraphs.push_back(i);
            }
        }
        // set omegas for gp
        for (auto &&v : *gp)
            v.schedule.getOmega()[2 * d] = unfusedOffset;
        SHOWLN(unfusedOffset);
        ++d;
        for (auto i : baseGraphs)
            if (optimize(graphs[i], sol, d, graphs[i].calcMaxDepth()))
                return true;
        // remove
        return false;
    }
    [[nodiscard]] bool optimizeLevel(Graph &g, Vector<Rational> &sol,
                                     size_t d) {
        if (numPhiCoefs) {
            instantiateOmniSimplex(g, d);
            addIndependentSolutionConstraints(g, d);
#ifndef NDEBUG
            {
                size_t i = 0;
                for (size_t eid = 0; eid < edges.size(); ++eid) {
                    // for (auto &e : edges) {
                    if (g.isInactive(eid, d))
                        continue;
                    const Dependence &e = edges[eid];
                    SHOW(e.depPoly.getNumLambda());
                    CSHOW(e.depPoly.getDim0());
                    CSHOW(e.depPoly.getDim1());
                    CSHOWLN(e.getNumPhiCoefficients());
                    std::cout << "constraints:\ndSat_" << i << " = "
                              << e.dependenceSatisfaction.tableau << "\ndBnd_"
                              << i << " = " << e.dependenceBounding.tableau
                              << std::endl;
                    ++i;
                }
            }
            SHOWLN(omniSimplex);
            SHOW(d);
            CSHOW(numBounding);
            CSHOW(numActiveEdges);
            CSHOW(numPhiCoefs);
            CSHOW(numOmegaCoefs);
            CSHOW(numLambda);
            CSHOWLN(numConstraints);
#endif
            if (omniSimplex.initiateFeasible())
                return true;
            sol.resizeForOverwrite(getLambdaOffset() - 1);
            omniSimplex.lexMinimize(sol);
            updateSchedules(g, sol, d);
            deactivateSatisfiedEdges(g, sol, d);
            // TODO: deactivate edges of satisfied dependencies
        } else {
            // TODO: something
            setSchedulesIndependent(g, d);
        }
        return false;
    }
    // optimize at depth `d`
    [[nodiscard]] bool optimize(Graph &g, Vector<Rational> &sol, size_t d,
                                size_t maxDepth) {
        if (d >= maxDepth)
            return false;
        countAuxParamsAndConstraints(g, d);
        setScheduleMemoryOffsets(g, d);
        SHOW(d);
        CSHOWLN(numPhiCoefs);
        // if we fail on this level, break the graph
        if (optimizeLevel(g, sol, d))
            return breakGraph(g, sol, d);
        // if we fail on some future level, break graph here
        if (optimize(g, sol, d + 1, maxDepth))
            return breakGraph(g, sol, d);
        // give up
        return false;
    }
    // returns true on failure
    [[nodiscard]] bool optimize() {
        fillEdges();
        fillUserToMemoryMap();
        connectGraph();
#ifndef NDEBUG
        validateMemory();
        validateEdges();
#endif
        Vector<Rational> sol;
        Graph g{fullGraph()};
        return optimize(g, sol, 0, calcMaxDepth());
    }
};

std::ostream &operator<<(std::ostream &os, const MemoryAccess &m) {
    if (m.isLoad)
        os << "= ";
    os << "ArrayReference:\n" << m.ref;
    if (!m.isLoad)
        os << " =";
    return os;
}
template <> struct std::iterator_traits<LoopBlock::Graph> {
    using difference_type = ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;
    using value_type = ScheduledNode;
    using reference_type = ScheduledNode &;
    using pointer_type = ScheduledNode *;
};
static_assert(std::ranges::range<LoopBlock::Graph>);
static_assert(Graphs::AbstractGraph<LoopBlock::Graph>);
