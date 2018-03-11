////////////////////////////////////////////////////////////////////////////////
//
// core/rbtree.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/intrusive/rbtree.hpp>
#include <amp/stddef.hpp>

#include <utility>


namespace amp {
namespace intrusive {
namespace aux {
namespace {

inline bool is_red(rbtree_node const* const x) noexcept
{ return x && (color(x) == rbtree_color::red); }

inline bool is_black(rbtree_node const* const x) noexcept
{ return !x || (color(x) == rbtree_color::black); }


inline void rotate_left(rbtree_node* const x, rbtree_node& head) noexcept
{
    auto const y = x->right;
    x->right = y->left;

    if (y->left) {
        set_parent(y->left, x);
    }

    auto const xp = parent(x);
    set_parent(y, xp);

    if (x == parent(&head)) {
        set_parent(&head, y);
    }
    else if (x == xp->left) {
        xp->left = y;
    }
    else {
        xp->right = y;
    }

    y->left = x;
    set_parent(x, y);
}

inline void rotate_right(rbtree_node* const x, rbtree_node& head) noexcept
{
    auto const y = x->left;
    x->left = y->right;

    if (y->right) {
        set_parent(y->right, x);
    }

    auto const xp = parent(x);
    set_parent(y, xp);

    if (x == parent(&head)) {
        set_parent(&head, y);
    }
    else if (x == xp->right) {
        xp->right = y;
    }
    else {
        xp->left = y;
    }

    y->right = x;
    set_parent(x, y);
}

}     // namespace <unnamed>


rbtree_node* iterator_next(rbtree_node* x) noexcept
{
    if (x->right) {
        return minimum(x->right);
    }

    auto y = parent(x);
    while (x == y->right) {
        x = std::exchange(y, parent(y));
    }
    return (x->right != y) ? y : x;
}

rbtree_node* iterator_prev(rbtree_node* x) noexcept
{
    if (is_red(x) && (parent(parent(x)) == x)) {
        return x->right;
    }
    if (x->left) {
        return maximum(x->left);
    }

    auto y = parent(x);
    while (x == y->left) {
        x = std::exchange(y, parent(y));
    }
    return y;
}

void insert_and_rebalance(rbtree_node* x, rbtree_node* const p,
                          rbtree_node& head, bool const link_left) noexcept
{
    x->left = nullptr;
    x->right = nullptr;
    set_color(x, rbtree_color::red);
    set_parent(x, p);

    if (p == &head) {
        head.left = x;
        head.right = x;
        set_parent(&head, x);
    }
    else if (link_left) {
        p->left = x;
        if (head.left == p) {
            head.left = x;
        }
    }
    else {
        p->right = x;
        if (head.right == p) {
            head.right = x;
        }
    }

    for (;;) {
        auto xp = parent(x);
        auto const xpp = parent(xp);

        if (xp == &head || is_black(xp) || xpp == &head) {
            break;
        }
        set_color(xpp, rbtree_color::red);

        auto const xp_is_left_child = (xp == xpp->left);
        auto const y = xp_is_left_child ? xpp->right : xpp->left;

        if (is_red(y)) {
            set_color(xp, rbtree_color::black);
            set_color(y, rbtree_color::black);
            x = xpp;
        }
        else {
            if (xp_is_left_child) {
                if (x == xp->right) {
                    rotate_left(xp, head);
                    xp = x;
                }
                rotate_right(xpp, head);
            }
            else {
                if (x == xp->left) {
                    rotate_right(xp, head);
                    xp = x;
                }
                rotate_left(xpp, head);
            }
            set_color(xp, rbtree_color::black);
            break;
        }
    }
    set_color(parent(&head), rbtree_color::black);
}

void erase_and_rebalance(rbtree_node* z, rbtree_node& head) noexcept
{
    decltype(z) x, y, xp;

    if (!z->left) {
        y = z;
        x = z->right;
    }
    else if (!z->right) {
        y = z;
        x = z->left;
    }
    else {
        y = minimum(z->right);
        x = y->right;
    }

    auto const zp = parent(z);
    auto const z_is_leftchild = (z == zp->left);

    if (y != z) {
        set_parent(z->left, y);
        y->left = z->left;

        if (y != z->right) {
            y->right = z->right;
            set_parent(z->right, y);

            xp = parent(y);
            if (x) {
                set_parent(x, xp);
            }
            xp->left = x;
        }
        else {
            xp = y;
        }

        set_parent(y, zp);
        if (zp == &head) {
            set_parent(&head, y);
        }
        else if (z_is_leftchild) {
            zp->left = y;
        }
        else {
            zp->right = y;
        }
    }
    else {
        xp = zp;
        if (x) {
            set_parent(x, xp);
        }

        if (zp == &head) {
            set_parent(&head, x);
        }
        else if (z_is_leftchild) {
            zp->left = x;
        }
        else {
            zp->right = x;
        }

        if (head.left == z) {
            AMP_ASSERT(!z->left);
            head.left = (z->right ? minimum(z->right) : zp);
        }
        if (head.right == z) {
            AMP_ASSERT(!z->right);
            head.right = (z->left ? maximum(z->left) : zp);
        }
    }
    AMP_ASSERT(!x || (parent(x) == xp));

    rbtree_color new_z_color;
    if (y != z) {
        new_z_color = color(y);
        set_color(y, color(z));
    }
    else {
        new_z_color = color(z);
    }

    if (new_z_color == rbtree_color::red) {
        return;
    }

    while ((xp != &head) && is_black(x)) {
        if (x == xp->left) {
            auto w = xp->right;
            if (is_red(w)) {
                set_color(w, rbtree_color::black);
                set_color(xp, rbtree_color::red);
                rotate_left(xp, head);
                w = xp->right;
            }

            if (is_black(w->left) && is_black(w->right)) {
                set_color(w, rbtree_color::red);
                x = xp;
                xp = parent(xp);
            }
            else {
                if (is_black(w->right)) {
                    set_color(w->left, rbtree_color::black);
                    set_color(w, rbtree_color::red);
                    rotate_right(w, head);
                    w = xp->right;
                }

                set_color(w, color(xp));
                set_color(xp, rbtree_color::black);

                if (w->right) {
                    set_color(w->right, rbtree_color::black);
                }
                rotate_left(xp, head);
                break;
            }
        }
        else {
            auto w = xp->left;
            if (is_red(w)) {
                set_color(w, rbtree_color::black);
                set_color(xp, rbtree_color::red);
                rotate_right(xp, head);
                w = xp->left;
            }

            if (is_black(w->right) && is_black(w->left)) {
                set_color(w, rbtree_color::red);
                x = xp;
                xp = parent(xp);
            }
            else {
                if (is_black(w->left)) {
                    set_color(w->right, rbtree_color::black);
                    set_color(w, rbtree_color::red);
                    rotate_left(w, head);
                    w = xp->left;
                }

                set_color(w, color(xp));
                set_color(xp, rbtree_color::black);

                if (w->left) {
                    set_color(w->left, rbtree_color::black);
                }
                rotate_right(xp, head);
                break;
            }
        }
    }

    if (x) {
        set_color(x, rbtree_color::black);
    }
}

}}}   // namespace amp::intrusive::aux

